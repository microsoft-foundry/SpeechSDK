// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Unit tests for the Berkeley socket adapter's connect loop.
//
// This suite previously contained only #if 0 blocks written against MicroMock,
// which was retired: between BEGIN_TEST_SUITE and END_TEST_SUITE every test was
// disabled, so the target built and ran zero tests. The cases below are the
// umock_c replacement, and they cover the behaviour socketio_win32_ut already
// covers on the Windows side so the two adapters are held to the same contract.
//
// What is asserted is the candidate loop: how many attempts are made, in which
// address family, how much time each one is granted, and whether a failing
// candidate stops the open. Those are the properties the per-address timeout
// exists to provide.
//
// The tests assert recorded behaviour rather than a strict umock call trace.
// The connect path makes several calls whose order is an implementation detail,
// and pinning the full trace would make the suite fail on harmless refactors
// instead of on a behaviour change.

// For syscall(), which the virtual clock below uses to reach the real clock.
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#ifdef __cplusplus
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#else
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#endif

#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "testrunnerswitcher.h"
#include "umock_c.h"
#include "umocktypes_charptr.h"

static void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

// gballoc.h redirects calloc/realloc as well as malloc once ENABLE_MOCKS is on.
// Without hooks these return NULL by default, which silently turns every
// allocation in this file into a null dereference.
static void* my_gballoc_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void* my_gballoc_realloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

static void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/optionhandler.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#undef ENABLE_MOCKS

#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/xio.h"

// The adapter's own per-address grant. Kept as a literal on purpose: if the
// product constant changes, these tests should fail and be re-read rather than
// silently follow it.
#define EXPECTED_PER_ADDRESS_TIMEOUT_MS 10000

// The adapter's stand-in for ETIMEDOUT when poll() runs out the grant. Mirrors
// SOCKETIO_POLL_TIMEOUT_ERROR in socketio_berkeley.c, which is file-local.
#define SOCKETIO_POLL_TIMEOUT_ERROR_CODE 110

#define PORT_NUM 80
#define HOSTNAME_ARG "hostname"

#define MAX_CANDIDATES 8

// How the mocked connect/poll pair should behave for each attempt, in order.
typedef enum ATTEMPT_OUTCOME_TAG
{
    ATTEMPT_TIMES_OUT,
    ATTEMPT_SUCCEEDS,
    ATTEMPT_REFUSED,
    // poll() is interrupted by a signal once (-1, EINTR) and then reports the
    // socket as settled; SO_ERROR reads 0.
    ATTEMPT_EINTR_THEN_SUCCEEDS,
    ATTEMPT_EINTR_THEN_TIMES_OUT,
    ATTEMPT_REPEATED_EINTR_THEN_TIMES_OUT,
    // poll() advances the virtual clock and is interrupted once, then times out.
    ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT,
    // poll() itself fails with EBADF (not a timeout, not EINTR).
    ATTEMPT_POLL_FAILS
} ATTEMPT_OUTCOME;

// Candidate "families" that make the mocked getaddrinfo hand back a malformed
// entry, so the adapter's pre-socket validation can be exercised.
#define CANDIDATE_INVALID_FAMILY (-1)   /* ai_family is AF_UNIX */
#define CANDIDATE_SHORT_ADDRLEN (-2)    /* AF_INET6 with a sockaddr_in-sized length */

static int g_candidate_families[MAX_CANDIDATES];
static size_t g_candidate_count;
static ATTEMPT_OUTCOME g_attempt_outcomes[MAX_CANDIDATES];

// Observed during the run.
static int g_connect_families[MAX_CANDIDATES];
static size_t g_connect_attempt_count;
static int g_poll_timeouts_ms[MAX_CANDIDATES];
static size_t g_poll_count;
static size_t g_poll_count_at_attempt_start;
static unsigned int g_interruption_seconds;
static int g_socket_domains[MAX_CANDIDATES];
static int g_socket_types[MAX_CANDIDATES];
static size_t g_socket_count;
static size_t g_v6only_cleared_count;
static int g_v6only_last_value;
static int g_last_addrinfo_family;
static int g_last_addrinfo_flags;
#ifndef __APPLE__
static int g_interface_ioctl_enabled;
static int g_dual_stack_interface_disappeared;
static size_t g_interface_address_query_count;
static size_t g_bind_to_device_count;
static char g_bound_interface_name[IFNAMSIZ];
#endif

static IO_OPEN_RESULT_DETAILED g_open_result;
static size_t g_open_complete_count;
static int g_getaddrinfo_result;

// A virtual CLOCK_MONOTONIC to place interruptions at the deadline's edges without
// sleeping. Off by default, so the tests that measure real time are unaffected.
static int g_virtual_clock_enabled;
static int64_t g_virtual_clock_ns;
static int64_t g_virtual_clock_advance_ns;
static size_t g_virtual_clock_call_count;
static size_t g_virtual_clock_failing_call;

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
    if (!g_virtual_clock_enabled || (clock_id != CLOCK_MONOTONIC))
    {
        return (int)syscall(SYS_clock_gettime, clock_id, tp);
    }

    g_virtual_clock_call_count++;
    if (g_virtual_clock_call_count == g_virtual_clock_failing_call)
    {
        errno = EINVAL;
        return -1;
    }

    tp->tv_sec = (time_t)(g_virtual_clock_ns / 1000000000LL);
    tp->tv_nsec = (long)(g_virtual_clock_ns % 1000000000LL);
    return 0;
}

// Enables the virtual clock and asserts that it is interposed. failing_call is the
// 1-based clock call that fails with EINVAL, or 0 for none.
static void use_virtual_clock(int64_t advance_on_interruption_ns, size_t failing_call)
{
    struct timespec now;

    g_virtual_clock_enabled = 1;
    g_virtual_clock_ns = 1000LL * 1000000000LL;
    g_virtual_clock_advance_ns = advance_on_interruption_ns;
    g_virtual_clock_call_count = 0;
    g_virtual_clock_failing_call = 0;

    ASSERT_ARE_EQUAL(int, 0, clock_gettime(CLOCK_MONOTONIC, &now));
    ASSERT_IS_TRUE(((int64_t)now.tv_sec * 1000000000LL + now.tv_nsec) == g_virtual_clock_ns, "clock_gettime() is not interposed");

    g_virtual_clock_call_count = 0;
    g_virtual_clock_failing_call = failing_call;
}

// Number of descriptors the process currently holds. Comparing this around an
// open is a direct check that failed candidates released their sockets - a
// stronger statement than counting calls to a mocked close().
static size_t open_fd_count(void)
{
    size_t count = 0;
    DIR* dir = opendir("/proc/self/fd");
    if (dir != NULL)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (entry->d_name[0] != '.')
            {
                count++;
            }
        }
        (void)closedir(dir);
    }
    return count;
}

static const SINGLYLINKEDLIST_HANDLE TEST_SINGLYLINKEDLIST_HANDLE = (SINGLYLINKEDLIST_HANDLE)0x4242;
static const void** list_items = NULL;
static size_t list_item_count = 0;
static bool singlylinkedlist_add_called = false;

static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

#ifndef __APPLE__
static char g_dual_stack_interface_name[] = "dual0";
static char g_ipv6_only_interface_name[] = "v6only0";
static struct if_nameindex g_test_interfaces[] =
{
    { 1, g_dual_stack_interface_name },
    { 2, g_ipv6_only_interface_name },
    { 0, NULL }
};

struct if_nameindex* if_nameindex(void)
{
    return g_test_interfaces;
}

void if_freenameindex(struct if_nameindex* interfaces)
{
    (void)interfaces;
}

int ioctl(int fd, unsigned long request, ...)
{
    int result;
    struct ifreq* ifr = NULL;
    va_list args;
    (void)fd;

    if (request == SIOCGIFADDR || request == SIOCGIFHWADDR)
    {
        va_start(args, request);
        ifr = va_arg(args, struct ifreq*);
        va_end(args);
    }

    if (!g_interface_ioctl_enabled)
    {
        errno = ENOTTY;
        result = -1;
    }
    else if (request == SIOCGIFADDR)
    {
        g_interface_address_query_count++;
        errno = EADDRNOTAVAIL;
        result = -1;
    }
    else if (request != SIOCGIFHWADDR || ifr == NULL)
    {
        errno = EINVAL;
        result = -1;
    }
    else if (g_dual_stack_interface_disappeared && strcmp(ifr->ifr_name, g_dual_stack_interface_name) == 0)
    {
        errno = ENODEV;
        result = -1;
    }
    else
    {
        static const unsigned char dual_stack_mac[] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
        static const unsigned char ipv6_only_mac[] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };
        const unsigned char* mac = NULL;

        if (strcmp(ifr->ifr_name, g_dual_stack_interface_name) == 0)
        {
            mac = dual_stack_mac;
        }
        else if (strcmp(ifr->ifr_name, g_ipv6_only_interface_name) == 0)
        {
            mac = ipv6_only_mac;
        }

        if (mac == NULL)
        {
            errno = ENODEV;
            result = -1;
        }
        else
        {
            (void)memset(&ifr->ifr_hwaddr, 0, sizeof(ifr->ifr_hwaddr));
            (void)memcpy(ifr->ifr_hwaddr.sa_data, mac, 6);
            result = 0;
        }
    }

    return result;
}
#endif

static ATTEMPT_OUTCOME current_attempt_outcome(void)
{
    ATTEMPT_OUTCOME result = ATTEMPT_TIMES_OUT;

    if ((g_connect_attempt_count > 0) && (g_connect_attempt_count <= MAX_CANDIDATES))
    {
        result = g_attempt_outcomes[g_connect_attempt_count - 1];
    }

    return result;
}

// Hands out a fresh real descriptor per attempt. It must be real because the
// adapter calls fcntl() on it, and fcntl is variadic so umock_c cannot mock it.
// A fresh one per call means the adapter's own close() genuinely releases it,
// which is what lets the leak check below mean something.
MOCK_FUNCTION_WITH_CODE(, int, socket, int, domain, int, type, int, protocol)
int new_fd = open("/dev/null", O_RDWR);
if (g_socket_count < MAX_CANDIDATES)
{
    g_socket_domains[g_socket_count] = domain;
    g_socket_types[g_socket_count] = type;
}
g_socket_count++;
MOCK_FUNCTION_END(new_fd)

// close() is deliberately NOT mocked. It is a libc symbol the whole process
// shares - the test runtime and stdio call it too - so interposing it here
// would route unrelated calls into umock_c, including calls made before
// umock_c_init. Descriptor hygiene is checked directly instead, by counting
// /proc/self/fd around the open.

MOCK_FUNCTION_WITH_CODE(, int, connect, int, sockfd, const struct sockaddr*, addr, socklen_t, addrlen)
if ((addr != NULL) && (g_connect_attempt_count < MAX_CANDIDATES))
{
    g_connect_families[g_connect_attempt_count] = addr->sa_family;
}
g_connect_attempt_count++;
g_poll_count_at_attempt_start = g_poll_count;
// Always report "in progress" so the adapter takes the poll path, which is
// where the per-address grant is applied.
errno = EINPROGRESS;
MOCK_FUNCTION_END(-1)

MOCK_FUNCTION_WITH_CODE(, int, poll, struct pollfd*, fds, nfds_t, nfds, int, timeout)
int poll_result;
ATTEMPT_OUTCOME outcome = current_attempt_outcome();
if (g_poll_count < MAX_CANDIDATES)
{
    g_poll_timeouts_ms[g_poll_count] = timeout;
}
g_poll_count++;
if (((outcome == ATTEMPT_EINTR_THEN_SUCCEEDS) && (g_poll_count - g_poll_count_at_attempt_start == 1)) ||
    ((outcome == ATTEMPT_EINTR_THEN_TIMES_OUT) && (g_poll_count - g_poll_count_at_attempt_start == 1)) ||
    ((outcome == ATTEMPT_REPEATED_EINTR_THEN_TIMES_OUT) && (g_poll_count - g_poll_count_at_attempt_start <= 3)))
{
    if (outcome != ATTEMPT_EINTR_THEN_SUCCEEDS)
    {
        (void)sleep(g_interruption_seconds);
    }
    errno = EINTR;
    poll_result = -1;
}
else if ((outcome == ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT) && (g_poll_count - g_poll_count_at_attempt_start == 1))
{
    g_virtual_clock_ns += g_virtual_clock_advance_ns;
    errno = EINTR;
    poll_result = -1;
}
else if (outcome == ATTEMPT_POLL_FAILS)
{
    errno = EBADF;
    poll_result = -1;
}
else
{
    // 0 means the grant ran out; anything positive means the socket settled and
    // the adapter goes on to read SO_ERROR.
    poll_result = ((outcome == ATTEMPT_TIMES_OUT) ||
        (outcome == ATTEMPT_EINTR_THEN_TIMES_OUT) ||
        (outcome == ATTEMPT_REPEATED_EINTR_THEN_TIMES_OUT) ||
        (outcome == ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT)) ? 0 : 1;
}
MOCK_FUNCTION_END(poll_result)

MOCK_FUNCTION_WITH_CODE(, int, getsockopt, int, sockfd, int, level, int, optname, void*, optval, socklen_t*, optlen)
if (optval != NULL)
{
    *(int*)optval = (current_attempt_outcome() == ATTEMPT_REFUSED) ? ECONNREFUSED : 0;
}
MOCK_FUNCTION_END(0)

MOCK_FUNCTION_WITH_CODE(, int, setsockopt, int, sockfd, int, level, int, optname, const void*, optval, socklen_t, optlen)
if ((level == IPPROTO_IPV6) && (optname == IPV6_V6ONLY) && (optval != NULL))
{
    g_v6only_cleared_count++;
    g_v6only_last_value = *(const int*)optval;
}
#ifndef __APPLE__
else if ((level == SOL_SOCKET) && (optname == SO_BINDTODEVICE) && (optval != NULL))
{
    size_t name_length = (optlen < sizeof(g_bound_interface_name) - 1) ? optlen : sizeof(g_bound_interface_name) - 1;
    (void)memcpy(g_bound_interface_name, optval, name_length);
    g_bound_interface_name[name_length] = '\0';
    g_bind_to_device_count++;
}
#endif
MOCK_FUNCTION_END(0)

MOCK_FUNCTION_WITH_CODE(, int, getaddrinfo, const char*, node, const char*, service, const struct addrinfo*, hints, struct addrinfo**, res)
size_t candidate_index;
struct addrinfo* head = NULL;
struct addrinfo* tail = NULL;
int getaddrinfo_result = g_getaddrinfo_result;
g_last_addrinfo_family = (hints != NULL) ? hints->ai_family : -1;
g_last_addrinfo_flags = (hints != NULL) ? hints->ai_flags : -1;
for (candidate_index = 0; (getaddrinfo_result == 0) && (candidate_index < g_candidate_count); candidate_index++)
{
    struct addrinfo* entry = (struct addrinfo*)calloc(1, sizeof(struct addrinfo));
    int family = g_candidate_families[candidate_index];
    entry->ai_family = (family == CANDIDATE_INVALID_FAMILY) ? AF_UNIX : ((family == CANDIDATE_SHORT_ADDRLEN) ? AF_INET6 : family);
    entry->ai_socktype = SOCK_STREAM;
    entry->ai_protocol = IPPROTO_TCP;
    if (family == CANDIDATE_SHORT_ADDRLEN)
    {
        // An AF_INET6 entry whose address is only sockaddr_in-sized.
        struct sockaddr_in6* sa6 = (struct sockaddr_in6*)calloc(1, sizeof(struct sockaddr_in6));
        sa6->sin6_family = AF_INET6;
        entry->ai_addr = (struct sockaddr*)sa6;
        entry->ai_addrlen = sizeof(struct sockaddr_in);
    }
    else if (family == AF_INET6)
    {
        struct sockaddr_in6* sa6 = (struct sockaddr_in6*)calloc(1, sizeof(struct sockaddr_in6));
        sa6->sin6_family = AF_INET6;
        entry->ai_addr = (struct sockaddr*)sa6;
        entry->ai_addrlen = sizeof(struct sockaddr_in6);
    }
    else
    {
        struct sockaddr_in* sa4 = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in));
        sa4->sin_family = AF_INET;
        entry->ai_addr = (struct sockaddr*)sa4;
        entry->ai_addrlen = sizeof(struct sockaddr_in);
    }
    if (head == NULL)
    {
        head = entry;
    }
    else
    {
        tail->ai_next = entry;
    }
    tail = entry;
}
*res = head;
MOCK_FUNCTION_END(getaddrinfo_result)

MOCK_FUNCTION_WITH_CODE(, void, freeaddrinfo, struct addrinfo*, res)
while (res != NULL)
{
    struct addrinfo* next = res->ai_next;
    free(res->ai_addr);
    free(res);
    res = next;
}
MOCK_FUNCTION_END()

static LIST_ITEM_HANDLE my_singlylinkedlist_get_head_item(SINGLYLINKEDLIST_HANDLE list)
{
    LIST_ITEM_HANDLE listHandle = NULL;
    (void)list;
    if (list_item_count > 0)
    {
        listHandle = (LIST_ITEM_HANDLE)list_items[0];
        list_item_count--;
    }
    return listHandle;
}

static LIST_ITEM_HANDLE my_singlylinkedlist_add(SINGLYLINKEDLIST_HANDLE list, const void* item)
{
    const void** items = (const void**)realloc((void*)list_items, (list_item_count + 1) * sizeof(item));
    (void)list;
    if (items != NULL)
    {
        list_items = items;
        list_items[list_item_count++] = item;
    }
    singlylinkedlist_add_called = true;
    return (LIST_ITEM_HANDLE)list_item_count;
}

static const void* my_singlylinkedlist_item_get_value(LIST_ITEM_HANDLE item_handle)
{
    return singlylinkedlist_add_called ? (const void*)item_handle : NULL;
}

static LIST_ITEM_HANDLE my_singlylinkedlist_find(SINGLYLINKEDLIST_HANDLE handle, LIST_MATCH_FUNCTION match_function, const void* match_context)
{
    size_t i;
    const void* found_item = NULL;
    (void)handle;
    for (i = 0; i < list_item_count; i++)
    {
        if (match_function((LIST_ITEM_HANDLE)list_items[i], match_context))
        {
            found_item = list_items[i];
            break;
        }
    }
    return (LIST_ITEM_HANDLE)found_item;
}

static void my_singlylinkedlist_destroy(SINGLYLINKEDLIST_HANDLE handle)
{
    (void)handle;
    free((void*)list_items);
    list_items = NULL;
    list_item_count = 0;
}

static void test_on_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
    (void)context;
    (void)buffer;
    (void)size;
}

static void test_on_io_open_complete(void* context, IO_OPEN_RESULT_DETAILED open_result)
{
    (void)context;
    g_open_complete_count++;
    g_open_result = open_result;
}

static void test_on_io_error(void* context)
{
    (void)context;
}

static OPTIONHANDLER_HANDLE test_OptionHandler_Create(pfCloneOption cloneOption, pfDestroyOption destroyOption, pfSetOption setOption)
{
    (void)cloneOption;
    (void)destroyOption;
    (void)setOption;
    return (OPTIONHANDLER_HANDLE)0x4243;
}

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%d", (int)error_code);
    ASSERT_FAIL(temp_str);
}

// Declares the addresses getaddrinfo should return, in order, and how each
// attempt against them should behave.
static void given_candidates(size_t count, const int* families, const ATTEMPT_OUTCOME* outcomes)
{
    size_t i;
    g_candidate_count = count;
    for (i = 0; i < count; i++)
    {
        g_candidate_families[i] = families[i];
        g_attempt_outcomes[i] = outcomes[i];
    }
}

static CONCRETE_IO_HANDLE create_socket_io(const char* hostname, int enable_ipv6)
{
    SOCKETIO_CONFIG socketConfig = { hostname, PORT_NUM, NULL, enable_ipv6 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    ASSERT_IS_NOT_NULL(ioHandle);
    return ioHandle;
}

BEGIN_TEST_SUITE(socketio_berkeley_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    result = umocktypes_charptr_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_UMOCK_ALIAS_TYPE(CONCRETE_IO_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SINGLYLINKEDLIST_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LIST_ITEM_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LIST_MATCH_FUNCTION, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LIST_ACTION_FUNCTION, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LIST_CONDITION_FUNCTION, void*);
    REGISTER_UMOCK_ALIAS_TYPE(OPTIONHANDLER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(pfCloneOption, void*);
    REGISTER_UMOCK_ALIAS_TYPE(pfDestroyOption, void*);
    REGISTER_UMOCK_ALIAS_TYPE(pfSetOption, void*);
    REGISTER_UMOCK_ALIAS_TYPE(socklen_t, unsigned int);
    REGISTER_UMOCK_ALIAS_TYPE(nfds_t, unsigned long);
    // Every pointer argument that appears in a mock needs a registered type, or
    // umock_c has no handler to copy/stringify it with when it records the call.
    REGISTER_UMOCK_ALIAS_TYPE(socklen_t*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(const void*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(struct pollfd*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(const struct sockaddr*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(struct addrinfo*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(struct addrinfo**, void*);
    REGISTER_UMOCK_ALIAS_TYPE(const struct addrinfo*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(const char*, char*);

    REGISTER_GLOBAL_MOCK_RETURN(singlylinkedlist_create, TEST_SINGLYLINKEDLIST_HANDLE);
    REGISTER_GLOBAL_MOCK_RETURN(singlylinkedlist_remove, 0);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_calloc, my_gballoc_calloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_realloc, my_gballoc_realloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);
    REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_get_head_item, my_singlylinkedlist_get_head_item);
    REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_add, my_singlylinkedlist_add);
    REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_item_get_value, my_singlylinkedlist_item_get_value);
    REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_find, my_singlylinkedlist_find);
    REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_destroy, my_singlylinkedlist_destroy);
    REGISTER_GLOBAL_MOCK_HOOK(OptionHandler_Create, test_OptionHandler_Create);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    if (TEST_MUTEX_ACQUIRE(g_testByTest))
    {
        ASSERT_FAIL("Could not acquire test serialization mutex.");
    }

    umock_c_reset_all_calls();

    memset(g_candidate_families, 0, sizeof(g_candidate_families));
    memset(g_attempt_outcomes, 0, sizeof(g_attempt_outcomes));
    memset(g_connect_families, 0, sizeof(g_connect_families));
    memset(g_poll_timeouts_ms, 0, sizeof(g_poll_timeouts_ms));
    memset(g_socket_domains, 0, sizeof(g_socket_domains));
    memset(g_socket_types, 0, sizeof(g_socket_types));
    g_candidate_count = 0;
    g_connect_attempt_count = 0;
    g_poll_count = 0;
    g_poll_count_at_attempt_start = 0;
    g_interruption_seconds = 1;
    g_socket_count = 0;
    g_v6only_cleared_count = 0;
    g_v6only_last_value = -1;
    g_last_addrinfo_family = -1;
    g_last_addrinfo_flags = -1;
#ifndef __APPLE__
    g_interface_ioctl_enabled = 0;
    g_dual_stack_interface_disappeared = 0;
    g_interface_address_query_count = 0;
    g_bind_to_device_count = 0;
    g_bound_interface_name[0] = '\0';
#endif
    list_item_count = 0;
    singlylinkedlist_add_called = false;
    g_open_result.result = IO_OPEN_CANCELLED;
    g_open_result.code = 0;
    g_open_complete_count = 0;
    g_getaddrinfo_result = 0;
    g_virtual_clock_enabled = 0;
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    g_virtual_clock_enabled = 0;
    TEST_MUTEX_RELEASE(g_testByTest);
}

/* Every resolved address is granted the whole per-address timeout. There is no
   budget divided between candidates, so the second attempt must be offered just
   as much time as the first. */
TEST_FUNCTION(socketio_open_grants_every_address_the_full_timeout)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    int result;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[1]);

    socketio_destroy(ioHandle);
}

/* However many black-holed addresses of one family come first, the family
   behind them still has to get its attempt. Each address has its own grant. */
TEST_FUNCTION(socketio_open_reaches_ipv4_after_two_blackholed_ipv6_candidates)
{
    const int families[] = { AF_INET6, AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT, ATTEMPT_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    int result;

    given_candidates(3, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)3, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[1]);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[2]);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[2]);

    socketio_destroy(ioHandle);
}

/* Three candidates exercise repeated per-address grants and ensure the later
   IPv4 candidate is not starved by earlier timeouts. */
TEST_FUNCTION(socketio_open_reaches_ipv4_after_three_blackholed_ipv6_candidates)
{
    const int families[] = { AF_INET6, AF_INET6, AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT, ATTEMPT_TIMES_OUT, ATTEMPT_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    int result;

    given_candidates(4, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)4, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[3]);
    // The last candidate is granted the same amount as the first: nothing was
    // deducted from it by the three attempts in front of it.
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[3]);

    socketio_destroy(ioHandle);
}

/* A refused address returns immediately and must not cost the addresses behind
   it anything - neither their attempt nor any part of their grant. */
TEST_FUNCTION(socketio_open_refused_address_does_not_reduce_the_next_grant)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_REFUSED, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    int result;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[1]);

    socketio_destroy(ioHandle);
}

/* When every candidate fails the open has to fail too and leave no socket
   behind. */
TEST_FUNCTION(socketio_open_fails_and_releases_every_socket_when_all_candidates_time_out)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT, ATTEMPT_TIMES_OUT };
    CONCRETE_IO_HANDLE ioHandle;
    int result;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    fds_before = open_fd_count();

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    // socketio_open returns 0 once it has invoked the callback - the outcome is
    // carried in the callback so upstream layers can surface the error code -
    // so the failure has to be read from the reported result, not the return.
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, SOCKETIO_POLL_TIMEOUT_ERROR_CODE, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    // Two sockets were created and both attempts failed, so the process must
    // hold no more descriptors than it did before the open.
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

#ifndef __APPLE__
TEST_FUNCTION(socketio_open_preserves_network_interface_enumeration_error)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);
    ASSERT_ARE_EQUAL(int, 0, socketio_setoption(ioHandle, OPTION_NET_INT_MAC_ADDRESS, "00:11:22:33:44:55"));

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, ENOTTY, g_open_result.code);

    socketio_destroy(ioHandle);
}
#endif

/* IPv4-mapped destinations such as ::ffff:203.0.113.1 resolve to AF_INET6 and
   can only be delivered by a dual-stack socket, so IPV6_V6ONLY has to be
   cleared - and only on AF_INET6 sockets, where the option means anything. */
TEST_FUNCTION(socketio_open_clears_ipv6_v6only_on_inet6_sockets_only)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    int result;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, 0, result);
    // Two candidates were tried but only one of them was AF_INET6.
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_v6only_cleared_count);
    ASSERT_ARE_EQUAL(int, 0, g_v6only_last_value);

    socketio_destroy(ioHandle);
}

/* A single resolved address is the common case and must still get the full
   grant - the loop must not treat "last candidate" as a special, smaller one. */
TEST_FUNCTION(socketio_open_single_address_gets_the_full_timeout)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT };
    CONCRETE_IO_HANDLE ioHandle;
    int result;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);

    socketio_destroy(ioHandle);
}

/* The first candidate working must end the loop: no further address is tried. */
TEST_FUNCTION(socketio_open_stops_at_the_first_candidate_that_connects)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    int result;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    result = socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_without_ipv6_opt_in_requests_ipv4_only)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 0);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_INET, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, 0, g_last_addrinfo_flags);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[0]);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_retrieveoptions_succeeds)
{
    CONCRETE_IO_HANDLE ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    OPTIONHANDLER_HANDLE options = socketio_get_interface_description()->concrete_io_retrieveoptions(ioHandle);

    ASSERT_ARE_EQUAL(void_ptr, (OPTIONHANDLER_HANDLE)0x4243, options);
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_with_ipv6_opt_in_requests_both_families)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_UNSPEC, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, 0, g_last_addrinfo_flags);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv6_literal_without_opt_in_requests_ipv4_only)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io("::1", 0);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_INET, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[0]);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_with_ipv6_opt_in_preserves_ipv4_fallback)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_REFUSED, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_UNSPEC, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[1]);

    socketio_destroy(ioHandle);
}

/* Literals follow the opt-in like hostnames do: without it the lookup is the
   pre-IPv6 AF_INET one, with it both families are requested. */
TEST_FUNCTION(socketio_open_ipv4_literal_without_opt_in_requests_ipv4_only)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io("127.0.0.1", 0);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_INET, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[0]);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv4_literal_with_opt_in_requests_both_families)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io("127.0.0.1", 1);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_UNSPEC, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[0]);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv6_literal_with_opt_in_requests_both_families)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io("::1", 1);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_UNSPEC, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv4_mapped_literal_without_opt_in_requests_ipv4_only)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io("::ffff:127.0.0.1", 0);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_INET, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_v6only_cleared_count);

    socketio_destroy(ioHandle);
}

/* With the opt-in, a mapped literal resolves to an AF_INET6 candidate, which only
   reaches its IPv4 destination from a dual-stack socket. */
TEST_FUNCTION(socketio_open_ipv4_mapped_literal_with_opt_in_uses_a_dual_stack_socket)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io("::ffff:127.0.0.1", 1);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, AF_UNSPEC, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_v6only_cleared_count);
    ASSERT_ARE_EQUAL(int, 0, g_v6only_last_value);

    socketio_destroy(ioHandle);
}

/* The opt-in belongs to the IO instance, not to one connection: an IO that is
   closed and opened again, as a WebSocket client does when it reconnects, keeps
   asking the resolver for both families every time and releases each socket. */
TEST_FUNCTION(socketio_open_keeps_the_ipv6_opt_in_across_close_and_reopen)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;
    int cycle;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    for (cycle = 0; cycle < 4; cycle++)
    {
        // Each open starts from the first scripted attempt again.
        g_connect_attempt_count = 0;
        g_last_addrinfo_family = -1;
        g_open_complete_count = 0;
        g_open_result.result = IO_OPEN_CANCELLED;

        ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

        ASSERT_ARE_EQUAL(int, AF_UNSPEC, g_last_addrinfo_family);
        ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
        ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
        ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);

        ASSERT_ARE_EQUAL(int, 0, socketio_close(ioHandle, NULL, NULL));
        ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());
    }

    socketio_destroy(ioHandle);
}

/* However many candidates are tried, the caller hears about the open once. */
TEST_FUNCTION(socketio_open_reports_one_open_complete_when_a_later_candidate_connects)
{
    const int families[] = { AF_INET6, AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT, ATTEMPT_REFUSED, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(3, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)3, g_connect_attempt_count);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_reports_one_open_complete_when_every_candidate_fails)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_REFUSED, ATTEMPT_TIMES_OUT };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* A failed lookup has no candidate to try: one error callback carrying the
   resolver's code, and no socket created. */
TEST_FUNCTION(socketio_open_dns_failure_reports_one_error_and_creates_no_socket)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    g_getaddrinfo_result = EAI_NONAME;
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, EAI_NONAME, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* A failed open leaves the instance closed and clean, so it can be opened again. */
TEST_FUNCTION(socketio_open_can_be_retried_after_every_candidate_failed)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME fail[] = { ATTEMPT_TIMES_OUT };
    const ATTEMPT_OUTCOME succeed[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, fail);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);

    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);

    g_connect_attempt_count = 0;
    g_poll_count = 0;
    given_candidates(1, families, succeed);
    (void)socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL);

    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);

    socketio_destroy(ioHandle);
}

/* A signal landing while the adapter waits for the connect must not fail the
   candidate: poll() is retried after EINTR and the connect completes. */
TEST_FUNCTION(socketio_open_retries_poll_after_eintr_and_connects)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_THEN_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_connect_attempt_count);
    // One interrupted wait and one completed wait on the same candidate.
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_poll_count);
    // The connected socket is kept open by the instance.
    ASSERT_ARE_EQUAL(size_t, fds_before + 1, open_fd_count());

    socketio_destroy(ioHandle);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());
}

TEST_FUNCTION(socketio_open_preserves_remaining_time_after_eintr_and_falls_back)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_THEN_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    g_interruption_seconds = 5;
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)3, g_poll_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[1] <= 5000);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[1] >= 4500);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[2]);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, fds_before + 1, open_fd_count());

    socketio_destroy(ioHandle);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());
}

TEST_FUNCTION(socketio_open_preserves_remaining_second_after_late_eintr)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_THEN_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    g_interruption_seconds = 9;
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)3, g_poll_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[1] <= 1000);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[1] >= 500);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[2]);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);

    socketio_destroy(ioHandle);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());
}

TEST_FUNCTION(socketio_open_repeated_eintr_does_not_reset_the_deadline)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_REPEATED_EINTR_THEN_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)5, g_poll_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[1] <= EXPECTED_PER_ADDRESS_TIMEOUT_MS - 900);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[2] <= EXPECTED_PER_ADDRESS_TIMEOUT_MS - 1900);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[3] <= EXPECTED_PER_ADDRESS_TIMEOUT_MS - 2900);
    ASSERT_IS_TRUE(g_poll_timeouts_ms[3] > 0);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[4]);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);

    socketio_destroy(ioHandle);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());
}

/* An interruption that ends past the deadline times the candidate out without
   another poll(); the next candidate still gets a full grant. */
TEST_FUNCTION(socketio_open_eintr_after_the_deadline_times_out_and_falls_back)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    use_virtual_clock(10500LL * 1000000LL, 0);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_poll_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[1]);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[1]);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, fds_before + 1, open_fd_count());

    socketio_destroy(ioHandle);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());
}

/* An interruption that ends exactly at the deadline leaves no time: it is reported
   as the per-address timeout, not as an interrupted or failed poll(). */
TEST_FUNCTION(socketio_open_eintr_at_the_deadline_reports_a_timeout)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    use_virtual_clock((int64_t)EXPECTED_PER_ADDRESS_TIMEOUT_MS * 1000000LL, 0);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_poll_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, SOCKETIO_POLL_TIMEOUT_ERROR_CODE, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* Four milliseconds is below the cutoff, so the adapter does not poll again. */
TEST_FUNCTION(socketio_open_does_not_poll_when_less_than_five_ms_remain)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    use_virtual_clock((int64_t)EXPECTED_PER_ADDRESS_TIMEOUT_MS * 1000000LL - 4000000LL, 0);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_poll_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, SOCKETIO_POLL_TIMEOUT_ERROR_CODE, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* Rounding up to the five-millisecond cutoff still gives poll() one last wait. */
TEST_FUNCTION(socketio_open_polls_when_rounded_remainder_is_five_ms)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    use_virtual_clock((int64_t)EXPECTED_PER_ADDRESS_TIMEOUT_MS * 1000000LL - 4500000LL, 0);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_poll_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_ARE_EQUAL(int, 5, g_poll_timeouts_ms[1]);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, SOCKETIO_POLL_TIMEOUT_ERROR_CODE, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* Without a monotonic clock there is no deadline: the candidate fails with the clock's
   error before any poll(), releases its socket, and the next gets a full grant. */
TEST_FUNCTION(socketio_open_clock_failure_before_poll_fails_the_candidate_and_falls_back)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_TIMES_OUT, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    use_virtual_clock(0, 1);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_poll_count);
    ASSERT_ARE_EQUAL(int, EXPECTED_PER_ADDRESS_TIMEOUT_MS, g_poll_timeouts_ms[0]);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, fds_before + 1, open_fd_count());

    socketio_destroy(ioHandle);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());
}

/* A clock failure after an interruption ends the candidate with the clock's error,
   instead of waiting again with a stale grant or reporting EINTR. */
TEST_FUNCTION(socketio_open_clock_failure_after_eintr_reports_the_clock_error)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_EINTR_ADVANCES_CLOCK_THEN_TIMES_OUT };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    use_virtual_clock(1000LL * 1000000LL, 2);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_poll_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, EINVAL, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* poll() failing outright (not a timeout, not EINTR) fails that candidate with
   poll's errno, releases its socket and lets the next candidate be tried. */
TEST_FUNCTION(socketio_open_poll_failure_fails_the_candidate_and_falls_back)
{
    const int families[] = { AF_INET6, AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_POLL_FAILS, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[1]);
    // Only the socket of the candidate that connected is still held.
    ASSERT_ARE_EQUAL(size_t, fds_before + 1, open_fd_count());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_poll_failure_on_the_only_candidate_reports_poll_errno)
{
    const int families[] = { AF_INET };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_POLL_FAILS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, EBADF, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* Malformed resolver entries are rejected before a socket is created for them,
   and a later valid candidate still gets its attempt. */
TEST_FUNCTION(socketio_open_skips_invalid_resolved_addresses_before_creating_a_socket)
{
    const int families[] = { CANDIDATE_INVALID_FAMILY, CANDIDATE_SHORT_ADDRLEN, AF_INET };
    // Outcomes are consumed per connect attempt; only the valid entry connects.
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS, ATTEMPT_SUCCEEDS, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(3, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_socket_count);
    ASSERT_ARE_EQUAL(int, AF_INET, g_socket_domains[0]);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(size_t, fds_before + 1, open_fd_count());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_with_only_invalid_resolved_addresses_fails_without_a_socket)
{
    const int families[] = { CANDIDATE_INVALID_FAMILY, CANDIDATE_SHORT_ADDRLEN };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS, ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(2, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_open_complete_count);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    // The last rejection reason is reported (EINVAL for the short address).
    ASSERT_ARE_EQUAL(int, EINVAL, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_socket_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

#ifndef __APPLE__
/* Interface enumeration failures retain their platform error and release both
   the connect socket and temporary hardware-query socket. */
TEST_FUNCTION(socketio_open_ipv6_interface_binding_releases_the_enumeration_socket_on_failure)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;
    size_t fds_before;

    given_candidates(1, families, outcomes);
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    ASSERT_ARE_EQUAL(int, 0, socketio_setoption(ioHandle, OPTION_NET_INT_MAC_ADDRESS, "00:11:22:33:44:55"));
    fds_before = open_fd_count();

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    // The mocked descriptors are /dev/null, so the enumeration ioctl fails with
    // ENOTTY; that platform error is what must be reported.
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, ENOTTY, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, (size_t)2, g_socket_count);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_socket_domains[0]);
    ASSERT_ARE_EQUAL(int, SOCK_STREAM, g_socket_types[0]);
    ASSERT_ARE_EQUAL(int, AF_INET, g_socket_domains[1]);
    ASSERT_ARE_EQUAL(int, SOCK_DGRAM, g_socket_types[1]);
    // Both the connect socket and the enumeration socket were released.
    ASSERT_ARE_EQUAL(size_t, fds_before, open_fd_count());

    socketio_destroy(ioHandle);
}

/* A queried hardware MAC selects a dual-addressed interface without using its
   IPv4 sockaddr bytes. */
TEST_FUNCTION(socketio_open_ipv6_interface_binding_uses_the_dual_stack_interface_hardware_mac)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    g_interface_ioctl_enabled = 1;
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    ASSERT_ARE_EQUAL(int, 0, socketio_setoption(ioHandle, OPTION_NET_INT_MAC_ADDRESS, "02:11:22:33:44:55"));

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_bind_to_device_count);
    ASSERT_ARE_EQUAL(char_ptr, g_dual_stack_interface_name, g_bound_interface_name);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_interface_address_query_count);

    socketio_destroy(ioHandle);
}

/* An interface with no IPv4 address remains selectable because enumeration
   does not request SIOCGIFADDR. */
TEST_FUNCTION(socketio_open_ipv6_interface_binding_finds_an_ipv6_only_interface)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    g_interface_ioctl_enabled = 1;
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    ASSERT_ARE_EQUAL(int, 0, socketio_setoption(ioHandle, OPTION_NET_INT_MAC_ADDRESS, "02:AA:BB:CC:DD:EE"));

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_bind_to_device_count);
    ASSERT_ARE_EQUAL(char_ptr, g_ipv6_only_interface_name, g_bound_interface_name);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_interface_address_query_count);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv6_interface_binding_skips_an_unrelated_disappeared_interface)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    g_interface_ioctl_enabled = 1;
    g_dual_stack_interface_disappeared = 1;
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    ASSERT_ARE_EQUAL(int, 0, socketio_setoption(ioHandle, OPTION_NET_INT_MAC_ADDRESS, "02:AA:BB:CC:DD:EE"));

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_bind_to_device_count);
    ASSERT_ARE_EQUAL(char_ptr, g_ipv6_only_interface_name, g_bound_interface_name);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_interface_address_query_count);

    socketio_destroy(ioHandle);
}

/* IPv4 sockaddr bytes that matched the old overwritten ifreq must not select
   either interface. */
TEST_FUNCTION(socketio_open_ipv6_interface_binding_rejects_overwritten_sockaddr_bytes)
{
    const int families[] = { AF_INET6 };
    const ATTEMPT_OUTCOME outcomes[] = { ATTEMPT_SUCCEEDS };
    CONCRETE_IO_HANDLE ioHandle;

    given_candidates(1, families, outcomes);
    g_interface_ioctl_enabled = 1;
    ioHandle = create_socket_io(HOSTNAME_ARG, 1);
    ASSERT_ARE_EQUAL(int, 0, socketio_setoption(ioHandle, OPTION_NET_INT_MAC_ADDRESS, "00:00:C0:00:02:02"));

    ASSERT_ARE_EQUAL(int, 0, socketio_open(ioHandle, test_on_io_open_complete, NULL, test_on_bytes_received, NULL, test_on_io_error, NULL));

    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, ENODEV, g_open_result.code);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_bind_to_device_count);
    ASSERT_ARE_EQUAL(size_t, (size_t)0, g_interface_address_query_count);

    socketio_destroy(ioHandle);
}
#endif

END_TEST_SUITE(socketio_berkeley_unittests)
