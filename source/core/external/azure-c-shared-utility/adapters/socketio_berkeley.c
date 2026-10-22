// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#define SOCKETIO_BERKELEY_UNDEF_BSD_SOURCE
#endif

#define _DEFAULT_SOURCE
#include <net/if.h>
#undef _DEFAULT_SOURCE

#ifdef SOCKETIO_BERKELEY_UNDEF_BSD_SOURCE
#undef _BSD_SOURCE
#undef SOCKETIO_BERKELEY_UNDEF_BSD_SOURCE
#endif

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "azure_c_shared_utility/socketio.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#ifdef TIZENRT
#include <net/lwip/tcp.h>
#include <tinyara/clock.h>
#else
#include <netinet/tcp.h>
#endif
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/gbnetwork.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/optionhandler.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/const_defines.h"
#include "azure_c_shared_utility/safe_math.h"
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKET_SUCCESS                 0
#define INVALID_SOCKET                 -1
#define MAC_ADDRESS_STRING_LENGTH      18

#ifndef IFREQ_BUFFER_SIZE
#define IFREQ_BUFFER_SIZE              1024
#endif

#define CONNECT_TIMEOUT_PER_ADDRESS_SECONDS 10
// Time allowed for a connect attempt against a single resolved address. There
// is no budget shared across addresses: each candidate gets this in full, so a
// blackholed address cannot deny the ones behind it their attempt.
#define CONNECT_TIMEOUT_PER_ADDRESS_MS (CONNECT_TIMEOUT_PER_ADDRESS_SECONDS * 1000)
#define MIN_CONNECT_POLL_REMAINING_MS 5
#define SOCKETIO_POLL_TIMEOUT_ERROR 110  /* ETIMEDOUT equivalent for poll timeout */

static int get_connect_time(struct timespec* now)
{
#if defined(TIZENRT) && !defined(CLOCK_MONOTONIC)
    int result = clock_systimespec(now);
    if (result != 0)
    {
        errno = -result;
        result = -1;
    }
    return result;
#else
    return clock_gettime(CLOCK_MONOTONIC, now);
#endif
}

typedef enum IO_STATE_TAG
{
    IO_STATE_CLOSED,
    IO_STATE_OPENING,
    IO_STATE_OPEN,
    IO_STATE_CLOSING,
    IO_STATE_ERROR
} IO_STATE;

typedef struct PENDING_SOCKET_IO_TAG
{
    unsigned char* bytes;
    size_t size;
    ON_SEND_COMPLETE on_send_complete;
    void* callback_context;
    SINGLYLINKEDLIST_HANDLE pending_io_list;
} PENDING_SOCKET_IO;

typedef struct SOCKET_IO_INSTANCE_TAG
{
    int socket;
    ON_BYTES_RECEIVED on_bytes_received;
    ON_IO_ERROR on_io_error;
    void* on_bytes_received_context;
    void* on_io_error_context;
    char* hostname;
    int port;
    int enable_ipv6;
    char* target_mac_address;
    IO_STATE io_state;
    SINGLYLINKEDLIST_HANDLE pending_io_list;
    unsigned char recv_bytes[RECEIVE_BYTES_VALUE];
} SOCKET_IO_INSTANCE;

typedef struct NETWORK_INTERFACE_DESCRIPTION_TAG
{
    char* name;
    char* mac_address;
    struct NETWORK_INTERFACE_DESCRIPTION_TAG* next;
} NETWORK_INTERFACE_DESCRIPTION;

/*this function will clone an option given by name and value*/
static void* socketio_CloneOption(const char* name, const void* value)
{
    void* result;

    if (name != NULL)
    {
        result = NULL;

        if (strcmp(name, OPTION_NET_INT_MAC_ADDRESS) == 0)
        {
            if (value == NULL)
            {
                LogError("Failed cloning option %s (value is NULL)", name);
            }
            else
            {
                size_t malloc_size = safe_add_size_t(strlen((char*)value), 1);
                malloc_size = safe_multiply_size_t(malloc_size, sizeof(char));
                if (malloc_size == SIZE_MAX)
                {
                    LogError("Invalid malloc size");
                }
                else if ((result = malloc(malloc_size)) == NULL)
                {
                    LogError("Failed cloning option %s (malloc failed)", name);
                }
                else if (strcpy((char*)result, (char*)value) == NULL)
                {
                    LogError("Failed cloning option %s (strcpy failed)", name);
                    free(result);
                    result = NULL;
                }
            }
        }
        else
        {
            LogError("Cannot clone option %s (not suppported)", name);
        }
    }
    else
    {
        result = NULL;
    }
    return result;
}

/*this function destroys an option previously created*/
static void socketio_DestroyOption(const char* name, const void* value)
{
    if (name != NULL)
    {
        if ((strcmp(name, OPTION_NET_INT_MAC_ADDRESS) == 0) && (value != NULL))
        {
            free((void*)value);
        }
    }
}

static OPTIONHANDLER_HANDLE socketio_retrieveoptions(CONCRETE_IO_HANDLE handle)
{
    OPTIONHANDLER_HANDLE result;

    if (handle == NULL)
    {
        LogError("failed retrieving options (handle is NULL)");
        result = NULL;
    }
    else
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)handle;

        result = OptionHandler_Create(socketio_CloneOption, socketio_DestroyOption, socketio_setoption);
        if (result == NULL)
        {
            LogError("unable to OptionHandler_Create");
        }
        else if (socket_io_instance->target_mac_address != NULL &&
            OptionHandler_AddOption(result, OPTION_NET_INT_MAC_ADDRESS, socket_io_instance->target_mac_address) != OPTIONHANDLER_OK)
        {
            LogError("failed retrieving options (failed adding net_interface_mac_address)");
            OptionHandler_Destroy(result);
            result = NULL;
        }
    }

    return result;
}

static const IO_INTERFACE_DESCRIPTION socket_io_interface_description =
{
    socketio_retrieveoptions,
    socketio_create,
    socketio_destroy,
    socketio_open,
    socketio_close,
    socketio_send,
    socketio_dowork,
    socketio_setoption
};

static void indicate_error(SOCKET_IO_INSTANCE* socket_io_instance)
{
    if (socket_io_instance->on_io_error != NULL)
    {
        socket_io_instance->on_io_error(socket_io_instance->on_io_error_context);
    }
}

static int add_pending_io(SOCKET_IO_INSTANCE* socket_io_instance, const unsigned char* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context)
{
    int result;
    PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)malloc(sizeof(PENDING_SOCKET_IO));
    if (pending_socket_io == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        pending_socket_io->bytes = (unsigned char*)malloc(size);
        if (pending_socket_io->bytes == NULL)
        {
            LogError("Allocation Failure: Unable to allocate pending list.");
            free(pending_socket_io);
            result = __FAILURE__;
        }
        else
        {
            pending_socket_io->size = size;
            pending_socket_io->on_send_complete = on_send_complete;
            pending_socket_io->callback_context = callback_context;
            pending_socket_io->pending_io_list = socket_io_instance->pending_io_list;
            (void)memcpy(pending_socket_io->bytes, buffer, size);

            if (singlylinkedlist_add(socket_io_instance->pending_io_list, pending_socket_io) == NULL)
            {
                LogError("Failure: Unable to add socket to pending list.");
                free(pending_socket_io->bytes);
                free(pending_socket_io);
                result = __FAILURE__;
            }
            else
            {
                result = 0;
            }
        }
    }
    return result;
}

static STATIC_VAR_UNUSED void signal_callback(int signum)
{
    AZURE_UNREFERENCED_PARAMETER(signum);
    LogError("Socket received signal %d.", signum);
}

#ifndef __APPLE__
static void destroy_network_interface_descriptions(NETWORK_INTERFACE_DESCRIPTION* nid)
{
    if (nid != NULL)
    {
        if (nid->next != NULL)
        {
            destroy_network_interface_descriptions(nid->next);
        }

        if (nid->name != NULL)
        {
            free(nid->name);
        }

        if (nid->mac_address != NULL)
        {
            free(nid->mac_address);
        }

        free(nid);
    }
}

static NETWORK_INTERFACE_DESCRIPTION* create_network_interface_description(const struct ifreq* ifr, NETWORK_INTERFACE_DESCRIPTION* previous_nid)
{
    NETWORK_INTERFACE_DESCRIPTION* result;
    size_t malloc_size = 0;

    if ((result = (NETWORK_INTERFACE_DESCRIPTION*)malloc(sizeof(NETWORK_INTERFACE_DESCRIPTION))) == NULL)
    {
        LogError("Failed allocating NETWORK_INTERFACE_DESCRIPTION");
    }
    else
    {
        const unsigned char* mac = (const unsigned char*)ifr->ifr_hwaddr.sa_data;
        result->name = NULL;
        result->mac_address = NULL;
        result->next = NULL;

        if ((malloc_size = safe_multiply_size_t(safe_add_size_t(strlen(ifr->ifr_name), 1), sizeof(char))) == SIZE_MAX)
        {
            LogError("invalid malloc size");
            destroy_network_interface_descriptions(result);
            result = NULL;
        }
        else if ((result->name = (char*)malloc(malloc_size)) == NULL)
        {
            LogError("failed setting interface description name (malloc failed)");
            destroy_network_interface_descriptions(result);
            result = NULL;
        }
        else if (strcpy(result->name, ifr->ifr_name) == NULL)
        {
            LogError("failed setting interface description name (strcpy failed)");
            destroy_network_interface_descriptions(result);
            result = NULL;
        }
        else if ((malloc_size = safe_multiply_size_t(sizeof(char), MAC_ADDRESS_STRING_LENGTH)) == SIZE_MAX ||
            (result->mac_address = (char*)malloc(malloc_size)) == NULL)
        {
            LogError("failed formatting mac address (malloc failed) size:%zu", malloc_size);
            destroy_network_interface_descriptions(result);
            result = NULL;
        }
        else if (sprintf(result->mac_address, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) <= 0)
        {
            LogError("failed formatting mac address (sprintf failed)");
            destroy_network_interface_descriptions(result);
            result = NULL;
        }
        else if (previous_nid != NULL)
        {
            previous_nid->next = result;
        }
    }

    return result;
}

static int add_network_interface_description(
    int socket,
    const char* interface_name,
    NETWORK_INTERFACE_DESCRIPTION** root_nid,
    NETWORK_INTERFACE_DESCRIPTION** previous_nid,
    int* error_code)
{
    int result;
    struct ifreq ifr;

    (void)memset(&ifr, 0, sizeof(ifr));
    if (strlen(interface_name) >= sizeof(ifr.ifr_name))
    {
        *error_code = ENAMETOOLONG;
        LogError("Network interface name is too long");
        result = __FAILURE__;
    }
    else
    {
        NETWORK_INTERFACE_DESCRIPTION* new_nid;
        (void)strcpy(ifr.ifr_name, interface_name);

        if (ioctl(socket, SIOCGIFHWADDR, &ifr) != 0)
        {
            *error_code = errno;
            LogError("ioctl failed querying socket (SIOCGIFHWADDR, errno=%d)", *error_code);
            result = __FAILURE__;
        }
        else if ((new_nid = create_network_interface_description(&ifr, *previous_nid)) == NULL)
        {
            *error_code = ENOMEM;
            LogError("Failed creating network interface description");
            result = __FAILURE__;
        }
        else
        {
            if (*root_nid == NULL)
            {
                *root_nid = new_nid;
            }
            *previous_nid = new_nid;
            result = 0;
        }
    }

    return result;
}

static int get_network_interface_descriptions(int socket, NETWORK_INTERFACE_DESCRIPTION** nid, int* error_code)
{
    int result = 0;
    NETWORK_INTERFACE_DESCRIPTION* root_nid = NULL;
    NETWORK_INTERFACE_DESCRIPTION* previous_nid = NULL;

#if defined(__linux__) && !defined(__ANDROID__)
    struct if_nameindex* interfaces = if_nameindex();

    if (interfaces == NULL)
    {
        *error_code = (errno == 0) ? ENODEV : errno;
        LogError("if_nameindex failed (errno=%d)", *error_code);
        result = __FAILURE__;
    }
    else
    {
        const struct if_nameindex* current_interface;

        for (current_interface = interfaces;
            current_interface->if_index != 0 && current_interface->if_name != NULL;
            current_interface++)
        {
            if (add_network_interface_description(
                socket, current_interface->if_name, &root_nid, &previous_nid, error_code) != 0)
            {
                // An unrelated interface can disappear after if_nameindex enumerates it.
                if (*error_code == ENODEV)
                {
                    continue;
                }
                result = __FAILURE__;
                break;
            }
        }

        if_freenameindex(interfaces);
    }
#else
    {
        struct ifconf ifc;
        char buf[IFREQ_BUFFER_SIZE];

        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;

        if (ioctl(socket, SIOCGIFCONF, &ifc) == -1)
        {
            *error_code = errno;
            LogError("ioctl failed querying socket (SIOCGIFCONF, errno=%d)", *error_code);
            result = __FAILURE__;
        }
        else
        {
            struct ifreq* current_interface = ifc.ifc_req;
            const struct ifreq* const end = current_interface + (ifc.ifc_len / sizeof(struct ifreq));

            for (; current_interface != end; current_interface++)
            {
                if (add_network_interface_description(
                    socket, current_interface->ifr_name, &root_nid, &previous_nid, error_code) != 0)
                {
                    result = __FAILURE__;
                    break;
                }
            }
        }
    }
#endif

    if (result == 0)
    {
        *nid = root_nid;
        *error_code = 0;
    }
    else
    {
        destroy_network_interface_descriptions(root_nid);
    }

    return result;
}

static int set_target_network_interface(int target_socket, char* mac_address, int* error_code)
{
    int result;
    int enumeration_socket;
    NETWORK_INTERFACE_DESCRIPTION* nid;

    // Interface names are address-family independent; the temporary socket is
    // used only to query each interface's hardware address.
    enumeration_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (enumeration_socket < SOCKET_SUCCESS)
    {
        *error_code = errno;
        LogError("Failed creating socket for network interface enumeration (%d)", *error_code);
        result = __FAILURE__;
    }
    else if (get_network_interface_descriptions(enumeration_socket, &nid, error_code) != 0)
    {
        LogError("Failed getting network interface descriptions");
        result = __FAILURE__;
    }
    else
    {
        NETWORK_INTERFACE_DESCRIPTION* current_nid = nid;

        while(current_nid != NULL)
        {
            if (strcmp(mac_address, current_nid->mac_address) == 0)
            {
                break;
            }

            current_nid = current_nid->next;
        }

        if (current_nid == NULL)
        {
            *error_code = ENODEV;
            LogError("Did not find a network interface matching MAC ADDRESS");
            result = __FAILURE__;
        }
        else if (setsockopt(target_socket, SOL_SOCKET, SO_BINDTODEVICE, current_nid->name, strlen(current_nid->name)) != 0)
        {
            *error_code = errno;
            LogError("setsockopt failed (%d)", *error_code);
            result = __FAILURE__;
        }
        else
        {
            *error_code = 0;
            result = 0;
        }

        destroy_network_interface_descriptions(nid);
    }

    if (enumeration_socket >= SOCKET_SUCCESS)
    {
        close(enumeration_socket);
    }

    return result;
}
#endif //__APPLE__

CONCRETE_IO_HANDLE socketio_create(void* io_create_parameters)
{
    SOCKETIO_CONFIG* socket_io_config = io_create_parameters;
    SOCKET_IO_INSTANCE* result;

    if (socket_io_config == NULL)
    {
        LogError("Invalid argument: socket_io_config is NULL");
        result = NULL;
    }
    else
    {
        result = malloc(sizeof(SOCKET_IO_INSTANCE));
        if (result != NULL)
        {
            result->pending_io_list = singlylinkedlist_create();
            if (result->pending_io_list == NULL)
            {
                LogError("Failure: singlylinkedlist_create unable to create pending list.");
                free(result);
                result = NULL;
            }
            else
            {
                if (socket_io_config->hostname != NULL)
                {
                    size_t malloc_size = safe_add_size_t(strlen(socket_io_config->hostname), 1);
                    if (malloc_size == SIZE_MAX)
                    {
                        LogError("invalid malloc size");
                        result->hostname = NULL;
                    }
                    else
                    {
                        result->hostname = (char*)malloc(malloc_size);
                        if (result->hostname != NULL)
                        {
                            (void)strcpy(result->hostname, socket_io_config->hostname);
                        }
                    }

                    result->socket = INVALID_SOCKET;
                }
                else
                {
                    result->hostname = NULL;
                    result->socket = *((int*)socket_io_config->accepted_socket);
                }

                if ((result->hostname == NULL) && (result->socket == INVALID_SOCKET))
                {
                    LogError("Failure: hostname == NULL and socket is invalid.");
                    singlylinkedlist_destroy(result->pending_io_list);
                    free(result);
                    result = NULL;
                }
                else
                {
                    result->port = socket_io_config->port;
                    result->enable_ipv6 = socket_io_config->enable_ipv6;
                    result->target_mac_address = NULL;
                    result->on_bytes_received = NULL;
                    result->on_io_error = NULL;
                    result->on_bytes_received_context = NULL;
                    result->on_io_error_context = NULL;
                    result->io_state = IO_STATE_CLOSED;
                }
            }
        }
        else
        {
            LogError("Allocation Failure: SOCKET_IO_INSTANCE");
        }
    }

    return result;
}

void socketio_destroy(CONCRETE_IO_HANDLE socket_io)
{
    if (socket_io != NULL)
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;
        /* we cannot do much if the close fails, so just ignore the result */
        if (socket_io_instance->socket != INVALID_SOCKET)
        {
            close(socket_io_instance->socket);
        }

        /* clear allpending IOs */
        LIST_ITEM_HANDLE first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list);
        while (first_pending_io != NULL)
        {
            PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)singlylinkedlist_item_get_value(first_pending_io);
            if (pending_socket_io != NULL)
            {
                free(pending_socket_io->bytes);
                free(pending_socket_io);
            }

            (void)singlylinkedlist_remove(socket_io_instance->pending_io_list, first_pending_io);
            first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list);
        }

        singlylinkedlist_destroy(socket_io_instance->pending_io_list);
        free(socket_io_instance->hostname);
        free(socket_io_instance->target_mac_address);
        free(socket_io);
    }
}

// Rejects a resolved address that cannot safely be handed to socket() and
// connect(). The caller skips it and moves on to the next candidate.
static int validate_addrinfo(const struct addrinfo* address, const char* hostname, int* error_code)
{
    int result = 0;

    if (address->ai_addr == NULL)
    {
        *error_code = EINVAL;
        LogError("Failure: resolved address is NULL for host %s.", hostname);
        result = __FAILURE__;
    }
    else if ((address->ai_family != AF_INET) && (address->ai_family != AF_INET6))
    {
        *error_code = EAFNOSUPPORT;
        LogError("Failure: unsupported address family %d for host %s.", address->ai_family, hostname);
        result = __FAILURE__;
    }
    else if (((address->ai_family == AF_INET) && (address->ai_addrlen < sizeof(struct sockaddr_in))) ||
             ((address->ai_family == AF_INET6) && (address->ai_addrlen < sizeof(struct sockaddr_in6))))
    {
        *error_code = EINVAL;
        LogError("Failure: resolved address length %zu is too short for host %s.",
            (size_t)address->ai_addrlen, hostname);
        result = __FAILURE__;
    }
    else if ((int)address->ai_addr->sa_family != address->ai_family)
    {
        *error_code = EINVAL;
        LogError("Failure: resolved address family %u does not match addrinfo family %d for host %s.",
            (unsigned int)address->ai_addr->sa_family, address->ai_family, hostname);
        result = __FAILURE__;
    }

    return result;
}

static int connect_to_addrinfo(SOCKET_IO_INSTANCE* socket_io_instance, const struct addrinfo* address, int timeout_ms, int* error_code)
{
    // Every branch below is a failure except the two that reach result = 0, and
    // the cleanup at the end keys off result, so default to failure.
    int result = __FAILURE__;
    int connect_result;
    int flags;
    char resolved_ip[INET6_ADDRSTRLEN] = { 0 };
    const void* resolved_address = NULL;

    socket_io_instance->socket = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (socket_io_instance->socket < SOCKET_SUCCESS)
    {
        *error_code = errno;
        LogError("Failure: socket create failure %d (%s).", *error_code, strerror(*error_code));
    }
#ifndef __APPLE__
    else if (socket_io_instance->target_mac_address != NULL &&
        set_target_network_interface(socket_io_instance->socket, socket_io_instance->target_mac_address, error_code) != 0)
    {
        LogError("Failure: failed selecting target network interface (MACADDR=%s, errno=%d).",
            socket_io_instance->target_mac_address, *error_code);
    }
#endif //__APPLE__
    else if ((-1 == (flags = fcntl(socket_io_instance->socket, F_GETFL, 0))) ||
        (fcntl(socket_io_instance->socket, F_SETFL, flags | O_NONBLOCK) == -1))
    {
        *error_code = errno;
        LogError("Failure: fcntl failure %d (%s).", *error_code, strerror(*error_code));
    }
    else
    {
        if (address->ai_family == AF_INET)
        {
            resolved_address = &((const struct sockaddr_in*)address->ai_addr)->sin_addr;
        }
        else if (address->ai_family == AF_INET6)
        {
            resolved_address = &((const struct sockaddr_in6*)address->ai_addr)->sin6_addr;
        }

        // An IPv4-mapped destination such as ::ffff:203.0.113.1 resolves to
        // AF_INET6 and can only be reached from a dual-stack socket. Linux
        // already allows this by default (net.ipv6.bindv6only=0), but the
        // sysctl can be flipped, so set it explicitly and stay aligned with
        // the Windows adapter. Not fatal: a failure here only means a mapped
        // destination may be refused later.
        if (address->ai_family == AF_INET6)
        {
            int v6only = 0;
            if (setsockopt(socket_io_instance->socket, IPPROTO_IPV6, IPV6_V6ONLY,
                &v6only, sizeof(v6only)) != 0)
            {
                LogInfo("Could not clear IPV6_V6ONLY (%d) for %s; IPv4-mapped destinations may be refused.",
                    errno, socket_io_instance->hostname);
            }
        }

        if ((resolved_address != NULL) &&
            (inet_ntop(address->ai_family, resolved_address, resolved_ip, sizeof(resolved_ip)) != NULL))
        {
            LogInfo("DNS resolved %s to %s, connecting to %s:%d (socket fd=%d)",
                socket_io_instance->hostname, resolved_ip, socket_io_instance->hostname,
                socket_io_instance->port, socket_io_instance->socket);
        }
        else
        {
            LogInfo("DNS resolved successfully, connecting to %s:%d (socket fd=%d)",
                socket_io_instance->hostname, socket_io_instance->port, socket_io_instance->socket);
        }

        connect_result = connect(socket_io_instance->socket, address->ai_addr, address->ai_addrlen);
        if ((connect_result != 0) && (errno != EINPROGRESS))
        {
            *error_code = errno;
            LogError("Failure: connect to %s:%d failed with error %d (%s).",
                socket_io_instance->hostname, socket_io_instance->port, *error_code, strerror(*error_code));
        }
        else if (connect_result != 0)
        {
            int poll_result;
            int poll_error = 0;
            int clock_error = 0;
            struct pollfd fd = { 0 };
            fd.fd = socket_io_instance->socket;
            fd.events = POLLOUT;

            LogInfo("Connect in progress (EINPROGRESS), waiting up to %d milliseconds for %s:%d",
                timeout_ms, socket_io_instance->hostname, socket_io_instance->port);

            struct timespec now;
            if (get_connect_time(&now) != 0)
            {
                poll_result = -1;
                poll_error = errno;
                clock_error = 1;
            }
            else
            {
                int64_t deadline_ns = (int64_t)now.tv_sec * 1000000000LL +
                    now.tv_nsec + (int64_t)timeout_ms * 1000000LL;
                int remaining_ms = timeout_ms;

                do
                {
                    poll_result = poll(&fd, 1, remaining_ms);
                    if (poll_result < 0)
                    {
                        poll_error = errno;
                        if (poll_error == EINTR)
                        {
                            int64_t remaining_ns;
                            if (get_connect_time(&now) != 0)
                            {
                                poll_error = errno;
                                clock_error = 1;
                                break;
                            }

                            remaining_ns = deadline_ns - ((int64_t)now.tv_sec * 1000000000LL + now.tv_nsec);
                            remaining_ms = (remaining_ns > 0) ?
                                (int)((remaining_ns + 999999LL) / 1000000LL) : 0;
                            if (remaining_ms < MIN_CONNECT_POLL_REMAINING_MS)
                            {
                                poll_result = 0;
                                break;
                            }
                        }
                    }
                } while ((poll_result < 0) && (poll_error == EINTR));
            }

            if (poll_result == 0)
            {
                *error_code = SOCKETIO_POLL_TIMEOUT_ERROR;
                LogError("Failure: connection timed out after %d milliseconds waiting for %s:%d.",
                    timeout_ms, socket_io_instance->hostname, socket_io_instance->port);
            }
            else if (poll_result < 0)
            {
                *error_code = poll_error;
                if (clock_error)
                {
                    LogError("Failure: monotonic clock failure %d (%s).", poll_error, strerror(poll_error));
                }
                else
                {
                    LogError("Failure: poll failure, retval %d, errno %d (%s).",
                        poll_result, poll_error, strerror(poll_error));
                }
            }
            else
            {
                int socket_error = 0;
                socklen_t socket_error_length = sizeof(socket_error);

                if (getsockopt(socket_io_instance->socket, SOL_SOCKET, SO_ERROR,
                    &socket_error, &socket_error_length) != 0)
                {
                    *error_code = errno;
                    LogError("Failure: getsockopt failure %d (%s).", *error_code, strerror(*error_code));
                }
                else if (socket_error != 0)
                {
                    *error_code = socket_error;
                    LogError("Failure: connect to %s:%d failed with error %d (%s).",
                        socket_io_instance->hostname, socket_io_instance->port,
                        socket_error, strerror(socket_error));
                }
                else
                {
                    result = 0;
                }
            }
        }
        else
        {
            result = 0;
        }
    }

    if (result != 0)
    {
        if (socket_io_instance->socket >= SOCKET_SUCCESS)
        {
            close(socket_io_instance->socket);
        }
        socket_io_instance->socket = INVALID_SOCKET;
    }
    else
    {
        *error_code = 0;
        LogInfo("TCP connection to %s:%d established successfully (fd=%d).",
            socket_io_instance->hostname, socket_io_instance->port, socket_io_instance->socket);
    }

    return result;
}

int socketio_open(CONCRETE_IO_HANDLE socket_io, ON_IO_OPEN_COMPLETE on_io_open_complete, void* on_io_open_complete_context, ON_BYTES_RECEIVED on_bytes_received, void* on_bytes_received_context, ON_IO_ERROR on_io_error, void* on_io_error_context)
{
    int result;

    IO_OPEN_RESULT_DETAILED open_result_detailed = { IO_OPEN_OK, 0 };

    SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;
    if (socket_io == NULL)
    {
        LogError("Invalid argument: SOCKET_IO_INSTANCE is NULL");
        result = open_result_detailed.code = __FAILURE__;
    }
    else
    {
        if (socket_io_instance->io_state != IO_STATE_CLOSED)
        {
            LogError("Failure: socket state is not closed.");
            result = open_result_detailed.code = __FAILURE__;
        }
        else if (socket_io_instance->socket != INVALID_SOCKET)
        {
            // Opening an accepted socket
            socket_io_instance->on_bytes_received_context = on_bytes_received_context;
            socket_io_instance->on_bytes_received = on_bytes_received;
            socket_io_instance->on_io_error = on_io_error;
            socket_io_instance->on_io_error_context = on_io_error_context;

            socket_io_instance->io_state = IO_STATE_OPEN;

            result = 0;
        }
        else
        {
            struct addrinfo* addrInfo;
            char portString[16];

            // Validate hostname before DNS resolution
            if (socket_io_instance->hostname == NULL || socket_io_instance->hostname[0] == '\0')
            {
                LogError("Failure: hostname is NULL or empty");
                open_result_detailed.code = __FAILURE__;
                result = __FAILURE__;
            }
            else
            {
                struct addrinfo addrHint = { 0 };
                // AF_UNSPEC asks for A and AAAA; AF_INET restores the IPv4-only
                // lookup this adapter did before IPv6 support was added. Apply
                // the opt-in to every host form, including IPv6 literals.
                addrHint.ai_family = (socket_io_instance->enable_ipv6 != 0) ? AF_UNSPEC : AF_INET;
                addrHint.ai_socktype = SOCK_STREAM;
                addrHint.ai_protocol = 0;
                // ai_flags is deliberately left clear. AI_ADDRCONFIG suppresses AAAA
                // whenever the host has no non-loopback IPv6 address - glibc does not
                // count loopback - which puts even the literal "::1" out of reach
                // (EAI_ADDRFAMILY) and demotes "::ffff:127.0.0.1" to AF_INET. What it
                // saves is nearly nothing: with no IPv6 route a connect fails
                // immediately with ENETUNREACH rather than timing out.

                sprintf(portString, "%u", socket_io_instance->port);
                LogInfo("Starting DNS lookup for %s:%d", socket_io_instance->hostname, socket_io_instance->port);
                int err = getaddrinfo(socket_io_instance->hostname, portString, &addrHint, &addrInfo);
                if (err != 0)
                {
                    LogError("Failure: getaddrinfo failure %d (%s) for host %s.", err, gai_strerror(err), socket_io_instance->hostname);
                    open_result_detailed.code = err;
                    result = __FAILURE__;
                }
                else
                {
                    int connect_error = __FAILURE__;
                    struct addrinfo* address;

                    result = __FAILURE__;
                    for (address = addrInfo; address != NULL; address = address->ai_next)
                    {
                        if (validate_addrinfo(address, socket_io_instance->hostname, &connect_error) != 0)
                        {
                            continue;
                        }

                        // Every candidate gets the same full grant. There is no
                        // budget shared across addresses, so a run of blackholed
                        // addresses in one family cannot exhaust the allowance and
                        // leave the other family - often the only one that works -
                        // unattempted. The cost is that the worst case grows with
                        // the number of resolved addresses rather than being capped.
                        if (connect_to_addrinfo(socket_io_instance, address, CONNECT_TIMEOUT_PER_ADDRESS_MS, &connect_error) == 0)
                        {
                            result = 0;
                            break;
                        }
                    }

                    if (result == 0)
                    {
                        socket_io_instance->on_bytes_received = on_bytes_received;
                        socket_io_instance->on_bytes_received_context = on_bytes_received_context;
                        socket_io_instance->on_io_error = on_io_error;
                        socket_io_instance->on_io_error_context = on_io_error_context;
                        socket_io_instance->io_state = IO_STATE_OPEN;
                    }
                    else
                    {
                        open_result_detailed.code = connect_error;
                    }

                    freeaddrinfo(addrInfo);
                }
            }
        }
    }

    if (on_io_open_complete != NULL)
    {
        open_result_detailed.result = result == 0 ? IO_OPEN_OK : IO_OPEN_ERROR;
        on_io_open_complete(on_io_open_complete_context, open_result_detailed);

        /* The xio_open contract is callback-based. Returning a failure code from here
           prevents upstream IO layers from surfacing open_result_detailed.code.
           Always return success once the callback has been invoked. */
        return 0;
    }

    return result;
}

int socketio_close(CONCRETE_IO_HANDLE socket_io, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* callback_context)
{
    int result = 0;

    if (socket_io == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;
        if ((socket_io_instance->io_state != IO_STATE_CLOSED) && (socket_io_instance->io_state != IO_STATE_CLOSING))
        {
            // Only close if the socket isn't already in the closed or closing state
            LogInfo("Closing socket (fd=%d)", socket_io_instance->socket);
            (void)shutdown(socket_io_instance->socket, SHUT_RDWR);
            close(socket_io_instance->socket);
            socket_io_instance->socket = INVALID_SOCKET;
            socket_io_instance->io_state = IO_STATE_CLOSED;
        }

        if (on_io_close_complete != NULL)
        {
            on_io_close_complete(callback_context);
        }

        result = 0;
    }

    return result;
}

int socketio_send(CONCRETE_IO_HANDLE socket_io, const void* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context)
{
    int result;

    if ((socket_io == NULL) ||
        (buffer == NULL) ||
        (size == 0))
    {
        /* Invalid arguments */
        LogError("Invalid argument: send given invalid parameter");
        result = __FAILURE__;
    }
    else
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;
        if (socket_io_instance->io_state != IO_STATE_OPEN)
        {
            LogError("Failure: socket state is not opened.");
            result = __FAILURE__;
        }
        else
        {
            LIST_ITEM_HANDLE first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list);
            if (first_pending_io != NULL)
            {
                if (add_pending_io(socket_io_instance, buffer, size, on_send_complete, callback_context) != 0)
                {
                    LogError("Failure: add_pending_io failed.");
                    result = __FAILURE__;
                }
                else
                {
                    result = 0;
                }
            }
            else
            {
                signal(SIGPIPE, SIG_IGN);

                ssize_t send_result = send(socket_io_instance->socket, buffer, size, 0);
                if (send_result != size)
                {
                    if (send_result == INVALID_SOCKET && errno != EAGAIN)
                    {
                        LogError("Failure: sending socket failed. errno=%d (%s).", errno, strerror(errno));
                        result = __FAILURE__;
                    }
                    else
                    {
                        if (send_result == INVALID_SOCKET && errno == EAGAIN) /*send says "come back later" with EAGAIN - likely the socket buffer cannot accept more data*/
                        {
                            // put the full message in the queue
                            send_result = 0;
                        }

                        /* queue remaining data */
                        if (add_pending_io(socket_io_instance, buffer + send_result, size - send_result, on_send_complete, callback_context) != 0)
                        {
                            LogError("Failure: add_pending_io failed.");
                            result = __FAILURE__;
                        }
                        else
                        {
                            result = 0;
                        }
                    }
                }
                else
                {
                    if (on_send_complete != NULL)
                    {
                        on_send_complete(callback_context, IO_SEND_OK);
                    }

                    result = 0;
                }
            }
        }
    }

    return result;
}

void socketio_dowork(CONCRETE_IO_HANDLE socket_io)
{
    if (socket_io != NULL)
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;
        LIST_ITEM_HANDLE first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list);
        while (first_pending_io != NULL)
        {
            PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)singlylinkedlist_item_get_value(first_pending_io);
            if (pending_socket_io == NULL)
            {
                socket_io_instance->io_state = IO_STATE_ERROR;
                indicate_error(socket_io_instance);
                LogError("Failure: retrieving socket from list");
                break;
            }

            signal(SIGPIPE, SIG_IGN);

            ssize_t send_result = send(socket_io_instance->socket, pending_socket_io->bytes, pending_socket_io->size, 0);
            if (send_result != pending_socket_io->size)
            {
                if (send_result == INVALID_SOCKET)
                {
                    if (errno == EAGAIN) /*send says "come back later" with EAGAIN - likely the socket buffer cannot accept more data*/
                    {
                        /*do nothing until next dowork */
                        break;
                    }
                    else
                    {
                        free(pending_socket_io->bytes);
                        free(pending_socket_io);
                        (void)singlylinkedlist_remove(socket_io_instance->pending_io_list, first_pending_io);

                        LogError("Failure: sending Socket information. errno=%d (%s).", errno, strerror(errno));
                        socket_io_instance->io_state = IO_STATE_ERROR;
                        indicate_error(socket_io_instance);
                    }
                }
                else
                {
                    /* simply wait until next dowork */
                    size_t remaining = pending_socket_io->size - send_result;
                    (void)memmove(pending_socket_io->bytes, pending_socket_io->bytes + send_result, remaining);
                    pending_socket_io->size = remaining;
                    break;
                }
            }
            else
            {
                if (pending_socket_io->on_send_complete != NULL)
                {
                    pending_socket_io->on_send_complete(pending_socket_io->callback_context, IO_SEND_OK);
                }

                free(pending_socket_io->bytes);
                free(pending_socket_io);
                if (singlylinkedlist_remove(socket_io_instance->pending_io_list, first_pending_io) != 0)
                {
                    socket_io_instance->io_state = IO_STATE_ERROR;
                    indicate_error(socket_io_instance);
                    LogError("Failure: unable to remove socket from list");
                }
            }

            first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list);
        }

        if (socket_io_instance->io_state == IO_STATE_OPEN)
        {
            ssize_t received = 0;
            do
            {
                received = recv(socket_io_instance->socket, socket_io_instance->recv_bytes, RECEIVE_BYTES_VALUE, 0);
                if (received > 0)
                {
                    if (socket_io_instance->on_bytes_received != NULL)
                    {
                        /* Explicitly ignoring here the result of the callback */
                        (void)socket_io_instance->on_bytes_received(socket_io_instance->on_bytes_received_context, socket_io_instance->recv_bytes, received);
                    }
                }
                else if (received == 0)
                {
                    // Do not log error here due to this is probably the socket being closed on the other end
                    LogInfo("Socket closed by peer (fd=%d)", socket_io_instance->socket);
                    indicate_error(socket_io_instance);
                }
                else if (received < 0 && errno != EAGAIN)
                {
                    LogError("Socketio_Failure: Receiving data from endpoint: errno=%d (%s).", errno, strerror(errno));
                    indicate_error(socket_io_instance);
                }

            } while (received > 0 && socket_io_instance->io_state == IO_STATE_OPEN);
        }
    }
}

// Edison is missing this from netinet/tcp.h, but this code still works if we manually define it.
#ifndef SOL_TCP
#define SOL_TCP 6
#endif

#ifndef __APPLE__
static void strtoup(char* str)
{
    if (str != NULL)
    {
        while (*str != '\0')
        {
            if (isalpha((int)*str) && islower((int)*str))
            {
                *str = (char)toupper((int)*str);
            }
            str++;
        }
    }
}
#endif // __APPLE__

int socketio_setoption(CONCRETE_IO_HANDLE socket_io, const char* optionName, const void* value)
{
    int result;
    size_t malloc_size = 0;

    if (socket_io == NULL ||
        optionName == NULL ||
        value == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;

        if (strcmp(optionName, "tcp_keepalive") == 0)
        {
            result = setsockopt(socket_io_instance->socket, SOL_SOCKET, SO_KEEPALIVE, value, sizeof(int));
            if (result == -1) result = errno;
        }
        else if (strcmp(optionName, "tcp_keepalive_time") == 0)
        {
#ifdef __APPLE__
            result = setsockopt(socket_io_instance->socket, IPPROTO_TCP, TCP_KEEPALIVE, value, sizeof(int));
#else
            result = setsockopt(socket_io_instance->socket, SOL_TCP, TCP_KEEPIDLE, value, sizeof(int));
#endif
            if (result == -1) result = errno;
        }
        else if (strcmp(optionName, "tcp_keepalive_interval") == 0)
        {
            result = setsockopt(socket_io_instance->socket, SOL_TCP, TCP_KEEPINTVL, value, sizeof(int));
            if (result == -1) result = errno;
        }
        else if (strcmp(optionName, OPTION_NET_INT_MAC_ADDRESS) == 0)
        {
#ifdef __APPLE__
            LogError("option not supported.");
            result = __FAILURE__;
#else
            if (strlen(value) == 0)
            {
                LogError("option value must be a valid mac address");
                result = __FAILURE__;
            }
            else if ((malloc_size = safe_multiply_size_t(sizeof(char), safe_add_size_t(strlen(value), 1))) == SIZE_MAX)
            {
                LogError("Invalid malloc size");
                result = __FAILURE__;
                socket_io_instance->target_mac_address = NULL;
            }
            else if ((socket_io_instance->target_mac_address = (char*)malloc(malloc_size)) == NULL)
            {
                LogError("failed setting net_interface_mac_address option (malloc failed)");
                result = __FAILURE__;
            }
            else if (strcpy(socket_io_instance->target_mac_address, value) == NULL)
            {
                LogError("failed setting net_interface_mac_address option (strcpy failed)");
                free(socket_io_instance->target_mac_address);
                socket_io_instance->target_mac_address = NULL;
                result = __FAILURE__;
            }
            else
            {
                strtoup(socket_io_instance->target_mac_address);
                result = 0;
            }
#endif
        }
        else if (strcmp(optionName, "tcp_nodelay") == 0)
        {
            result = setsockopt(socket_io_instance->socket, IPPROTO_TCP, TCP_NODELAY, value, sizeof(int));
            if (result == -1) result = errno;
        }
        else
        {
            result = __FAILURE__;
        }
    }

    return result;
}

const IO_INTERFACE_DESCRIPTION* socketio_get_interface_description(void)
{
    return &socket_io_interface_description;
}
