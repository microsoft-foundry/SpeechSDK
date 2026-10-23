// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mstcpip.h>
#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/gbnetwork.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/safe_math.h"

// Time allowed for a connect attempt against a single resolved address. There
// is no budget shared across addresses: each candidate gets this in full, so a
// blackholed address cannot deny the ones behind it their attempt.
#define CONNECT_TIMEOUT_PER_ADDRESS_MS 10000

typedef enum IO_STATE_TAG
{
    IO_STATE_CLOSED,
    IO_STATE_OPENING,
    IO_STATE_OPEN,
    IO_STATE_CLOSING
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
    SOCKET socket;
    ON_BYTES_RECEIVED on_bytes_received;
    ON_IO_ERROR on_io_error;
    void* on_bytes_received_context;
    void* on_io_error_context;
    char* hostname;
    int port;
    int enable_ipv6;
    IO_STATE io_state;
    SINGLYLINKEDLIST_HANDLE pending_io_list;
    struct tcp_keepalive keep_alive;
    unsigned char recv_bytes[RECEIVE_BYTES_VALUE];
} SOCKET_IO_INSTANCE;

/*this function will clone an option given by name and value*/
static void* socketio_CloneOption(const char* name, const void* value)
{
    void* result = NULL;

    if ((name == NULL) || (value == NULL))
    {
        LogError("Failed cloning option (name or value is NULL)");
    }
    else
    {
        LogError("Cannot clone option %s (not supported)", name);
    }

    return result;
}

/*this function destroys an option previously created*/
static void socketio_DestroyOption(const char* name, const void* value)
{
    (void)name;
    (void)value;
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
        result = OptionHandler_Create(socketio_CloneOption, socketio_DestroyOption, socketio_setoption);
        if (result == NULL)
        {
            LogError("unable to OptionHandler_Create");
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

CONCRETE_IO_HANDLE socketio_create(void* io_create_parameters)
{
    SOCKETIO_CONFIG* socket_io_config = (SOCKETIO_CONFIG*)io_create_parameters;
    SOCKET_IO_INSTANCE* result;
    struct tcp_keepalive tcp_keepalive = { 0, 0, 0 };

    if (socket_io_config == NULL)
    {
        LogError("Invalid argument: socket_io_config is NULL");
        result = NULL;
    }
    else
    {
        result = (SOCKET_IO_INSTANCE*)malloc(sizeof(SOCKET_IO_INSTANCE));
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
                        LogError("Invalid malloc size");
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
                    result->socket = *((SOCKET*)socket_io_config->accepted_socket);
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
                    result->on_bytes_received = NULL;
                    result->on_io_error = NULL;
                    result->on_bytes_received_context = NULL;
                    result->on_io_error_context = NULL;
                    result->io_state = IO_STATE_CLOSED;
                    result->keep_alive = tcp_keepalive;

                }
            }
        }
        else
        {
            LogError("Allocation Failure: SOCKET_IO_INSTANCE");
        }
    }

    return (XIO_HANDLE)result;
}

void socketio_destroy(CONCRETE_IO_HANDLE socket_io)
{
    LIST_ITEM_HANDLE first_pending_io;

    if (socket_io != NULL)
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;
        /* we cannot do much if the close fails, so just ignore the result */
        (void)closesocket(socket_io_instance->socket);

        /* clear allpending IOs */

        while ((first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list)) != NULL)
        {
            PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)singlylinkedlist_item_get_value(first_pending_io);
            if (pending_socket_io != NULL)
            {
                free(pending_socket_io->bytes);
                free(pending_socket_io);
            }

            singlylinkedlist_remove(socket_io_instance->pending_io_list, first_pending_io);
        }

        singlylinkedlist_destroy(socket_io_instance->pending_io_list);
        if (socket_io_instance->hostname != NULL)
        {
            free(socket_io_instance->hostname);
        }

        free(socket_io);
    }
}

// Rejects a resolved address that cannot safely be handed to socket() and
// connect(). The caller skips it and moves on to the next candidate.
static int validate_addrinfo(const ADDRINFO* addr, const char* hostname, int* error_code)
{
    int result = 0;

    if (addr->ai_addr == NULL)
    {
        *error_code = WSAEINVAL;
        LogError("Failure: resolved address is NULL for host %s.", hostname);
        result = __FAILURE__;
    }
    else if ((addr->ai_family != AF_INET) && (addr->ai_family != AF_INET6))
    {
        *error_code = WSAEAFNOSUPPORT;
        LogError("Failure: unsupported address family %d for host %s.", addr->ai_family, hostname);
        result = __FAILURE__;
    }
    else if (((addr->ai_family == AF_INET) && (addr->ai_addrlen < sizeof(struct sockaddr_in))) ||
             ((addr->ai_family == AF_INET6) && (addr->ai_addrlen < sizeof(struct sockaddr_in6))))
    {
        *error_code = WSAEINVAL;
        LogError("Failure: resolved address length %llu is too short for host %s.",
            (unsigned long long)addr->ai_addrlen, hostname);
        result = __FAILURE__;
    }
    else if (addr->ai_addrlen > (size_t)INT_MAX)
    {
        *error_code = WSAEINVAL;
        LogError("Failure: resolved address length %llu does not fit connect for host %s.",
            (unsigned long long)addr->ai_addrlen, hostname);
        result = __FAILURE__;
    }
    else if (addr->ai_addr->sa_family != addr->ai_family)
    {
        *error_code = WSAEINVAL;
        LogError("Failure: resolved address family %u does not match addrinfo family %d for host %s.",
            (unsigned int)addr->ai_addr->sa_family, addr->ai_family, hostname);
        result = __FAILURE__;
    }

    return result;
}

// Attempt to connect to a single resolved address. On success returns 0 with the
// socket open and non-blocking; on failure returns __FAILURE__, closes the socket,
// sets it to INVALID_SOCKET, and records the Winsock error in *error_code.
static int connect_to_addrinfo(SOCKET_IO_INSTANCE* socket_io_instance, ADDRINFO* addr, int timeout_ms, int* error_code)
{
    // Every branch below is a failure except the two that reach result = 0, and
    // the cleanup at the end keys off result, so default to failure.
    int result = __FAILURE__;
    const char* hostname = (socket_io_instance->hostname != NULL) ? socket_io_instance->hostname : "<unknown>";

    socket_io_instance->socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (socket_io_instance->socket == INVALID_SOCKET)
    {
        *error_code = WSAGetLastError();
        LogError("Failure: socket create failure %d for %s.", *error_code, hostname);
    }
    else
    {
        u_long nonblocking = 1;

        // Windows defaults an AF_INET6 socket to v6-only, which refuses an
        // IPv4-mapped destination such as ::ffff:203.0.113.1 with
        // WSAEADDRNOTAVAIL before any packet is sent. Clear the option so a
        // mapped literal reaches its IPv4 destination, matching Linux, where
        // the kernel default (net.ipv6.bindv6only=0) already allows it.
        // Only meaningful on an AF_INET6 socket; a failure here is not fatal.
        if (addr->ai_family == AF_INET6)
        {
            int v6only = 0;
            if (setsockopt(socket_io_instance->socket, IPPROTO_IPV6, IPV6_V6ONLY,
                (const char*)&v6only, sizeof(v6only)) != 0)
            {
                LogInfo("Could not clear IPV6_V6ONLY (%d) for %s; IPv4-mapped destinations may be refused.",
                    WSAGetLastError(), hostname);
            }
        }

        if (ioctlsocket(socket_io_instance->socket, FIONBIO, &nonblocking) != 0)
        {
            *error_code = WSAGetLastError();
            LogError("Failure: ioctlsocket failure %d for %s:%d.", *error_code, hostname, socket_io_instance->port);
        }
        else
        {
            char resolved_ip[INET6_ADDRSTRLEN] = { 0 };
            const char* resolved_ip_str = NULL;
            if (addr->ai_family == AF_INET && addr->ai_addr != NULL)
            {
                struct sockaddr_in* sin = (struct sockaddr_in*)addr->ai_addr;
                resolved_ip_str = InetNtopA(AF_INET, &sin->sin_addr, resolved_ip, sizeof(resolved_ip));
            }
            else if (addr->ai_family == AF_INET6 && addr->ai_addr != NULL)
            {
                struct sockaddr_in6* sin6 = (struct sockaddr_in6*)addr->ai_addr;
                resolved_ip_str = InetNtopA(AF_INET6, &sin6->sin6_addr, resolved_ip, sizeof(resolved_ip));
            }

            if (resolved_ip_str != NULL)
            {
                LogInfo("DNS resolved %s to %s, connecting to %s:%d", hostname, resolved_ip_str, hostname, socket_io_instance->port);
            }
            else
            {
                LogInfo("DNS resolved successfully, connecting to %s:%d", hostname, socket_io_instance->port);
            }

            if (connect(socket_io_instance->socket, addr->ai_addr, (int)addr->ai_addrlen) == 0)
            {
                result = 0;
            }
            else
            {
                int connect_error = WSAGetLastError();
                if ((connect_error != WSAEWOULDBLOCK) &&
                    (connect_error != WSAEINPROGRESS) &&
                    (connect_error != WSAEALREADY))
                {
                    *error_code = connect_error;
                    LogError("Failure: connect to %s:%d failed with error %d.", hostname, socket_io_instance->port, *error_code);
                }
                else
                {
                    fd_set write_fds;
                    fd_set except_fds;
                    struct timeval timeout;
                    FD_ZERO(&write_fds);
                    FD_ZERO(&except_fds);
                    FD_SET(socket_io_instance->socket, &write_fds);
                    FD_SET(socket_io_instance->socket, &except_fds);
                    timeout.tv_sec = timeout_ms / 1000;
                    timeout.tv_usec = (timeout_ms % 1000) * 1000;

                    LogInfo("Connect in progress, waiting up to %d milliseconds for %s:%d",
                        timeout_ms, hostname, socket_io_instance->port);

                    int select_result = select(0, NULL, &write_fds, &except_fds, &timeout);
                    if (select_result == 0)
                    {
                        *error_code = WSAETIMEDOUT;
                        LogError("Failure: connection timed out after %d milliseconds waiting for %s:%d.",
                            timeout_ms, hostname, socket_io_instance->port);
                    }
                    else if (select_result == SOCKET_ERROR)
                    {
                        *error_code = WSAGetLastError();
                        LogError("Failure: select failed with error %d for %s:%d.",
                            *error_code, hostname, socket_io_instance->port);
                    }
                    else
                    {
                        int socket_error = 0;
                        int socket_error_length = sizeof(socket_error);
                        if (getsockopt(socket_io_instance->socket, SOL_SOCKET, SO_ERROR,
                            (char*)&socket_error, &socket_error_length) == SOCKET_ERROR)
                        {
                            *error_code = WSAGetLastError();
                            LogError("Failure: getsockopt failed with error %d for %s:%d.",
                                *error_code, hostname, socket_io_instance->port);
                        }
                        else if (socket_error != 0)
                        {
                            *error_code = socket_error;
                            LogError("Failure: connect to %s:%d failed with error %d.",
                                hostname, socket_io_instance->port, *error_code);
                        }
                        else
                        {
                            result = 0;
                        }
                    }
                }
            }
        }
    }

    if (result != 0)
    {
        if (socket_io_instance->socket != INVALID_SOCKET)
        {
            (void)closesocket(socket_io_instance->socket);
        }
        socket_io_instance->socket = INVALID_SOCKET;
    }
    else
    {
        LogInfo("TCP connection to %s:%d established successfully.", hostname, socket_io_instance->port);
        *error_code = 0;
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
        const char* hostname = (socket_io_instance->hostname != NULL) ? socket_io_instance->hostname : "<unknown>";
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
        else if (socket_io_instance->hostname == NULL || socket_io_instance->hostname[0] == '\0')
        {
            LogError("Failure: hostname is NULL or empty");
            result = open_result_detailed.code = __FAILURE__;
        }
        else
        {
            char portString[16];
            ADDRINFO addrHint = { 0 };
            ADDRINFO* addrInfo = NULL;

            // AF_UNSPEC asks for A and AAAA; AF_INET restores the IPv4-only
            // lookup this adapter did before IPv6 support was added. Apply
            // the opt-in to every host form, including IPv6 literals.
            addrHint.ai_family = (socket_io_instance->enable_ipv6 != 0) ? AF_UNSPEC : AF_INET;
            addrHint.ai_socktype = SOCK_STREAM;
            addrHint.ai_protocol = 0;
            // ai_flags is deliberately left clear, matching socketio_berkeley.c.
            // AI_ADDRCONFIG is measured to suppress AAAA on glibc whenever the host
            // has no non-loopback IPv6 address, putting even "::1" out of reach; the
            // Winsock threshold is unverified. Either way the flag buys nothing here,
            // since ai_family already names the families the caller asked for.
            sprintf(portString, "%d", socket_io_instance->port);
            LogInfo("Starting DNS lookup for %s:%d", hostname, socket_io_instance->port);
            int addrResult = getaddrinfo(socket_io_instance->hostname, portString, &addrHint, &addrInfo);
            if (addrResult != 0)
            {
                open_result_detailed.code = addrResult;
                LogError("Failure: getaddrinfo failure %d (%s) for host %s.", open_result_detailed.code, gai_strerrorA(open_result_detailed.code), hostname);
                result = __FAILURE__;
            }
            else
            {
                // getaddrinfo can return several addresses (e.g. AAAA then A).
                // Try each in turn and keep the first that connects.
                int connect_error = __FAILURE__;
                result = __FAILURE__;
                for (ADDRINFO* rp = addrInfo; rp != NULL; rp = rp->ai_next)
                {
                    if (validate_addrinfo(rp, hostname, &connect_error) != 0)
                    {
                        continue;
                    }

                    // Every candidate gets the same full grant. There is no
                    // budget shared across addresses, so a run of blackholed
                    // addresses in one family cannot exhaust the allowance and
                    // leave the other family - often the only one that works -
                    // unattempted. The cost is that the worst case grows with
                    // the number of resolved addresses rather than being capped.
                    if (connect_to_addrinfo(socket_io_instance, rp, CONNECT_TIMEOUT_PER_ADDRESS_MS, &connect_error) == 0)
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
    int result;

    if (socket_io == NULL)
    {
        LogError("Invalid argument: socket_io is NULL");
        result = __FAILURE__;
    }
    else
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;

        if ((socket_io_instance->io_state != IO_STATE_CLOSING) &&
            (socket_io_instance->io_state != IO_STATE_CLOSED))
        {
            (void)closesocket(socket_io_instance->socket);
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
                if (add_pending_io(socket_io_instance, (const unsigned char*)buffer, size, on_send_complete, callback_context) != 0)
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
                /* TODO: we need to do more than a cast here to be 100% clean
                The following bug was filed: [WarnL4] socketio_win32 does not account for already sent bytes and there is a truncation of size from size_t to int */
                int send_result = send(socket_io_instance->socket, (const char*)buffer, (int)size, 0);
                if (send_result != (int)size)
                {
                    int last_error = WSAGetLastError();
                    if (last_error != WSAEWOULDBLOCK)
                    {
                        LogError("Failure: sending socket failed %d.", last_error);
                        result = __FAILURE__;
                    }
                    else
                    {
                        /* queue data */
                        if (add_pending_io(socket_io_instance, (const unsigned char*)buffer, size, on_send_complete, callback_context) != 0)
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
    int send_result;
    if (socket_io != NULL)
    {
        SOCKET_IO_INSTANCE* socket_io_instance = (SOCKET_IO_INSTANCE*)socket_io;
        if (socket_io_instance->io_state == IO_STATE_OPEN)
        {
            LIST_ITEM_HANDLE first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list);
            while (first_pending_io != NULL)
            {
                PENDING_SOCKET_IO* pending_socket_io = (PENDING_SOCKET_IO*)singlylinkedlist_item_get_value(first_pending_io);
                if (pending_socket_io == NULL)
                {
                    LogError("Failure: retrieving socket from list");
                    indicate_error(socket_io_instance);
                    break;
                }

                /* TODO: we need to do more than a cast here to be 100% clean
                The following bug was filed: [WarnL4] socketio_win32 does not account for already sent bytes and there is a truncation of size from size_t to int */
                send_result = send(socket_io_instance->socket, (const char*)pending_socket_io->bytes, (int)pending_socket_io->size, 0);
                if (send_result != (int)pending_socket_io->size)
                {
                    int last_error = WSAGetLastError();
                    if (last_error != WSAEWOULDBLOCK)
                    {
                        free(pending_socket_io->bytes);
                        free(pending_socket_io);
                        (void)singlylinkedlist_remove(socket_io_instance->pending_io_list, first_pending_io);
                    }
                    else
                    {
                        /* try again */
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
                        LogError("Failure: removing socket from list");
                        indicate_error(socket_io_instance);
                    }
                }

                first_pending_io = singlylinkedlist_get_head_item(socket_io_instance->pending_io_list);
            }

            if (socket_io_instance->io_state == IO_STATE_OPEN)
            {
                int received = 0;
                do
                {
                    received = recv(socket_io_instance->socket, (char*)socket_io_instance->recv_bytes, RECEIVE_BYTES_VALUE, 0);
                    if ((received > 0))
                    {
                        if (socket_io_instance->on_bytes_received != NULL)
                        {
                            /* Explicitly ignoring here the result of the callback */
                            (void)socket_io_instance->on_bytes_received(socket_io_instance->on_bytes_received_context, socket_io_instance->recv_bytes, received);
                        }
                    }
                    else if (received == 0)
                    {
                        indicate_error(socket_io_instance);
                    }
                    else
                    {
                        int last_error = WSAGetLastError();
                        if (last_error != WSAEWOULDBLOCK && last_error != ERROR_SUCCESS)
                        {
                            LogError("Socketio_Failure: Receiving data from endpoint: %d.", last_error);
                            indicate_error(socket_io_instance);
                        }
                    }
                } while (received > 0 && socket_io_instance->io_state == IO_STATE_OPEN);
            }
        }
    }
}

static int set_keepalive(SOCKET_IO_INSTANCE* socket_io, struct tcp_keepalive* keepAlive)
{
    int result;
    DWORD bytesReturned;

    int err = WSAIoctl(socket_io->socket, SIO_KEEPALIVE_VALS, keepAlive,
        sizeof(struct tcp_keepalive), NULL, 0, &bytesReturned, NULL, NULL);
    if (err != 0)
    {
        LogError("Failure: setting keep-alive on the socket: %d.\r\n", err == SOCKET_ERROR ? WSAGetLastError() : err);
        result = __FAILURE__;
    }
    else
    {
        socket_io->keep_alive = *keepAlive;
        result = 0;
    }

    return result;
}

int socketio_setoption(CONCRETE_IO_HANDLE socket_io, const char* optionName, const void* value)
{
    int result;

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
            struct tcp_keepalive keepAlive = socket_io_instance->keep_alive;
            keepAlive.onoff = *(int *)value;

            result = set_keepalive(socket_io_instance, &keepAlive);
        }
        else if (strcmp(optionName, "tcp_keepalive_time") == 0)
        {
            unsigned long kaTime = *(int *)value * 1000; // convert to ms
            struct tcp_keepalive keepAlive = socket_io_instance->keep_alive;
            keepAlive.keepalivetime = kaTime;

            result = set_keepalive(socket_io_instance, &keepAlive);
        }
        else if (strcmp(optionName, "tcp_keepalive_interval") == 0)
        {
            unsigned long kaInterval = *(int *)value * 1000; // convert to ms
            struct tcp_keepalive keepAlive = socket_io_instance->keep_alive;
            keepAlive.keepaliveinterval = kaInterval;

            result = set_keepalive(socket_io_instance, &keepAlive);
        }
        else if (strcmp(optionName, "tcp_nodelay") == 0)
        {
            result = setsockopt(socket_io_instance->socket, IPPROTO_TCP, TCP_NODELAY, value, sizeof(int));
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
