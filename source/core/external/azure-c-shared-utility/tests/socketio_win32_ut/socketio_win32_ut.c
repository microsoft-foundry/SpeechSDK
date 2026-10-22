// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "testrunnerswitcher.h"

#undef DECLSPEC_IMPORT

#pragma warning(disable: 4273)

#include <winsock2.h>
#include <mstcpip.h>
#include <winsock2.h>
#include <ws2tcpip.h>

static size_t currentmalloc_call;
static size_t whenShallmalloc_fail;

void* my_gballoc_malloc(size_t size)
{
    void* result;
    currentmalloc_call++;
    if (whenShallmalloc_fail > 0)
    {
        if (currentmalloc_call == whenShallmalloc_fail)
        {
            result = NULL;
        }
        else
        {
            result = malloc(size);
        }
    }
    else
    {
        result = malloc(size);
    }
    return result;
}

void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS
#include "azure_c_shared_utility/optionhandler.h"
#undef ENABLE_MOCKS

#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/socketio.h"

#define ENABLE_MOCKS

#include "umock_c.h"
#include "umocktypes_charptr.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
static bool g_addrinfo_call_fail;
static bool g_addrinfo_two_addresses;
/* When set, getaddrinfo returns two IPv6 candidates ahead of one IPv4
   candidate - the ordering a real resolver commonly returns for a dual-stack
   host. */
static bool g_addrinfo_dual_stack;
static int g_last_addrinfo_family;
static int g_last_select_timeout_ms;
/* What socketio_open asked of IPV6_V6ONLY, so a test can check the value and
   not only that the option was touched. */
static size_t g_v6only_set_count;
static int g_v6only_last_value;
/* Records the address family of every connect() attempt, so a test can assert
   which families were actually tried and in what order. */
#define MAX_RECORDED_CONNECTS 8
static int g_connect_families[MAX_RECORDED_CONNECTS];
static size_t g_connect_attempt_count;
static int g_socket_error;
static IO_OPEN_RESULT_DETAILED g_open_result;
//static int g_socket_send_size_value;
static int g_socket_recv_size_value;

static const SINGLYLINKEDLIST_HANDLE TEST_SINGLYLINKEDLIST_HANDLE = (SINGLYLINKEDLIST_HANDLE)0x4242;
static const LIST_ITEM_HANDLE TEST_LIST_ITEM_HANDLE = (LIST_ITEM_HANDLE)0x11;
static const void** list_items = NULL;
static size_t list_item_count = 0;
static SOCKET test_socket = (SOCKET)0x4243;
static size_t list_head_count = 0;
static bool singlylinkedlist_add_called = false;
static size_t callbackContext = 11;
/* sa_family must agree with ai_family below, as it does for a real getaddrinfo
   result; socketio_open rejects a candidate whose families disagree. */
static const struct sockaddr test_sock_addr = { AF_INET, { 0 } };
static ADDRINFO TEST_ADDR_INFO = { AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP, 128, NULL, (struct sockaddr*)&test_sock_addr, NULL };
static const struct sockaddr test_sock_addr6 = { AF_INET6, { 0 } };
static ADDRINFO TEST_ADDR_INFO6 = { AI_PASSIVE, AF_INET6, SOCK_STREAM, IPPROTO_TCP, 128, NULL, (struct sockaddr*)&test_sock_addr6, NULL };

static const char* TEST_BUFFER_VALUE = "test_buffer_value";

#define PORT_NUM 80
#define HOSTNAME_ARG "hostname"

#define TEST_BUFFER_SIZE    17
#define TEST_CALLBACK_CONTEXT   0x951753

static struct tcp_keepalive persisted_tcp_keepalive;
typedef char* SOCKET_MUTABLE_BUFFER;
typedef const char* SOCKET_CONST_BUFFER;

static OPTIONHANDLER_HANDLE test_OptionHandler_Create(pfCloneOption cloneOption, pfDestroyOption destroyOption, pfSetOption setOption)
{
    (void)cloneOption;
    (void)destroyOption;
    (void)setOption;
    return (OPTIONHANDLER_HANDLE)0x4244;
}

MOCK_FUNCTION_WITH_CODE(WSAAPI, SOCKET, socket, int, af, int, type, int, protocol)
MOCK_FUNCTION_END(test_socket)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, closesocket, SOCKET, s)
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, connect, SOCKET, s, const struct sockaddr*, name, int, namelen)
if ((name != NULL) && (g_connect_attempt_count < MAX_RECORDED_CONNECTS))
{
    g_connect_families[g_connect_attempt_count++] = name->sa_family;
}
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, select, int, nfds, fd_set*, readfds, fd_set*, writefds, fd_set*, exceptfds, const struct timeval*, timeout)
if (timeout != NULL)
{
    g_last_select_timeout_ms = (int)(timeout->tv_sec * 1000 + timeout->tv_usec / 1000);
}
MOCK_FUNCTION_END(1)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, getsockopt, SOCKET, s, int, level, int, optname, SOCKET_MUTABLE_BUFFER, optval, int*, optlen)
if (optval != NULL)
{
    *(int*)optval = g_socket_error;
}
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, setsockopt, SOCKET, s, int, level, int, optname, SOCKET_CONST_BUFFER, optval, int, optlen)
if ((level == IPPROTO_IPV6) && (optname == IPV6_V6ONLY) && (optval != NULL))
{
    g_v6only_set_count++;
    g_v6only_last_value = *(const int*)optval;
}
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(WSAAPI, PCSTR, inet_ntop, INT, family, const VOID*, address, PSTR, buffer, size_t, buffer_size)
MOCK_FUNCTION_END(NULL)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, recv, SOCKET, s, SOCKET_MUTABLE_BUFFER, buf, int, len, int, flags)
    if (g_socket_recv_size_value >= 0)
    {
        len = g_socket_recv_size_value;
    }
MOCK_FUNCTION_END(-1)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, send, SOCKET, s, SOCKET_CONST_BUFFER, buf, int, len, int, flags)
/*if (g_socket_send_size_value >= 0)
{
len = g_socket_send_size_value;
}*/
MOCK_FUNCTION_END(len)
MOCK_FUNCTION_WITH_CODE(WSAAPI, INT, getaddrinfo, PCSTR, pNodeName, PCSTR, pServiceName, const ADDRINFOA*, pHints, PADDRINFOA*, ppResult)
int callFail;
if (pHints != NULL)
{
    g_last_addrinfo_family = pHints->ai_family;
}
if (!g_addrinfo_call_fail)
{
    *ppResult = (PADDRINFOA)malloc(sizeof(ADDRINFOA));
    memcpy(*ppResult, &TEST_ADDR_INFO, sizeof(ADDRINFOA));
    if (g_addrinfo_dual_stack)
    {
        /* IPv6, IPv6, IPv4 - the IPv4 candidate verifies sequential fallback
           after each preceding address receives its own connect grant. */
        memcpy(*ppResult, &TEST_ADDR_INFO6, sizeof(ADDRINFOA));
        (*ppResult)->ai_next = (PADDRINFOA)malloc(sizeof(ADDRINFOA));
        memcpy((*ppResult)->ai_next, &TEST_ADDR_INFO6, sizeof(ADDRINFOA));
        (*ppResult)->ai_next->ai_next = (PADDRINFOA)malloc(sizeof(ADDRINFOA));
        memcpy((*ppResult)->ai_next->ai_next, &TEST_ADDR_INFO, sizeof(ADDRINFOA));
        (*ppResult)->ai_next->ai_next->ai_next = NULL;
    }
    else if (g_addrinfo_two_addresses)
    {
        (*ppResult)->ai_next = (PADDRINFOA)malloc(sizeof(ADDRINFOA));
        memcpy((*ppResult)->ai_next, &TEST_ADDR_INFO, sizeof(ADDRINFOA));
        (*ppResult)->ai_next->ai_next = NULL;
    }
    callFail = 0;
}
else
{
    *ppResult = NULL;
    callFail = __FAILURE__;
}
MOCK_FUNCTION_END(callFail)
MOCK_FUNCTION_WITH_CODE(WSAAPI, void, freeaddrinfo, PADDRINFOA, pResult)
while (pResult != NULL)
{
    PADDRINFOA next = pResult->ai_next;
    free(pResult);
    pResult = next;
}
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, WSAGetLastError)
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, ioctlsocket, SOCKET, s, long, cmd, u_long FAR*, argp)
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(WSAAPI, int, WSAIoctl, SOCKET, s, DWORD, dwIoControlCode, LPVOID, lpvInBuffer, DWORD, cbInBuffer, LPVOID, lpvOutBuffer, DWORD, cbOutBuffer, LPDWORD, lpcbBytesReturned, LPWSAOVERLAPPED, lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE, lpCompletionRoutine)
(void)memcpy(&persisted_tcp_keepalive, lpvInBuffer, sizeof(struct tcp_keepalive));
MOCK_FUNCTION_END(0)

LIST_ITEM_HANDLE my_singlylinkedlist_get_head_item(SINGLYLINKEDLIST_HANDLE list)
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

LIST_ITEM_HANDLE my_singlylinkedlist_add(SINGLYLINKEDLIST_HANDLE list, const void* item)
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

const void* my_singlylinkedlist_item_get_value(LIST_ITEM_HANDLE item_handle)
{
    const void* resultPtr = NULL;
    if (singlylinkedlist_add_called)
    {
        resultPtr = item_handle;
    }

    return (const void*)resultPtr;
}

LIST_ITEM_HANDLE my_singlylinkedlist_find(SINGLYLINKEDLIST_HANDLE handle, LIST_MATCH_FUNCTION match_function, const void* match_context)
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

void my_singlylinkedlist_destroy(SINGLYLINKEDLIST_HANDLE handle)
{
    (void)handle;
    free((void*)list_items);
    list_items = NULL;
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
    g_open_result = open_result;
}

static void test_on_io_close_complete(void* context)
{
    (void)context;
}

static void test_on_io_error(void* context)
{
    (void)context;
}

char* umocktypes_stringify_const_ADDRINFOA_ptr(const ADDRINFOA** value)
{
    char* result = NULL;
    char temp_buffer[256];
    int length;

    length = sprintf(temp_buffer, "{ ai_flags = %d, ai_family = %d, ai_socktype = %d, ai_protocol = %d, ai_addrlen = %u, ai_canonname = %s", (*value)->ai_flags, (*value)->ai_family, (*value)->ai_socktype, (*value)->ai_protocol, (unsigned int)((*value)->ai_addrlen), (*value)->ai_canonname);
    if (length > 0)
    {
        result = (char*)malloc(strlen(temp_buffer) + 1);
        if (result != NULL)
        {
            (void)memcpy(result, temp_buffer, strlen(temp_buffer) + 1);
        }
    }

    return result;
}

int umocktypes_are_equal_const_ADDRINFOA_ptr(const ADDRINFOA** left, const ADDRINFOA** right)
{
    int result = 1;
    if (((*left)->ai_flags != (*right)->ai_flags) ||
        ((*left)->ai_family != (*right)->ai_family) ||
        ((*left)->ai_socktype != (*right)->ai_socktype) ||
        ((*left)->ai_protocol != (*right)->ai_protocol) ||
        ((((*left)->ai_canonname == NULL) || ((*right)->ai_canonname == NULL)) && ((*left)->ai_canonname != (*right)->ai_canonname)) ||
        (strcmp((*left)->ai_canonname, (*right)->ai_canonname) != 0))
    {
        result = 0;
    }

    return result;
}

int umocktypes_copy_const_ADDRINFOA_ptr(ADDRINFOA** destination, const ADDRINFOA** source)
{
    int result;

    *destination = (ADDRINFOA*)malloc(sizeof(ADDRINFOA));
    if (*destination == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        (*destination)->ai_flags = (*source)->ai_flags;
        (*destination)->ai_family = (*source)->ai_family;
        (*destination)->ai_socktype = (*source)->ai_socktype;
        (*destination)->ai_protocol = (*source)->ai_protocol;
        (*destination)->ai_canonname = (*source)->ai_canonname;

        result = 0;
    }

    return result;
}

void umocktypes_free_const_ADDRINFOA_ptr(ADDRINFOA** value)
{
    free(*value);
}

char* umocktypes_stringify_const_struct_sockaddr_ptr(const struct sockaddr** value)
{
    char* result = NULL;
    char temp_buffer[256];
    int length;

    length = sprintf(temp_buffer, "{ sa_family = %u, sa_data = ... }", (unsigned int)((*value)->sa_family));
    if (length > 0)
    {
        result = (char*)malloc(strlen(temp_buffer) + 1);
        if (result != NULL)
        {
            (void)memcpy(result, temp_buffer, strlen(temp_buffer) + 1);
        }
    }

    return result;
}

int umocktypes_are_equal_const_struct_sockaddr_ptr(const struct sockaddr** left, const struct sockaddr** right)
{
    int result = 1;
    if (((*left)->sa_family != (*left)->sa_family) ||
        (memcmp((*left)->sa_data, (*right)->sa_data, sizeof((*left)->sa_data) != 0)))
    {
        result = 0;
    }

    return result;
}

int umocktypes_copy_const_struct_sockaddr_ptr(struct sockaddr** destination, const struct sockaddr** source)
{
    int result;

    *destination = (struct sockaddr*)malloc(sizeof(struct sockaddr));
    if (*destination == NULL)
    {
        result = __FAILURE__;
    }
    else
    {
        (*destination)->sa_family = (*source)->sa_family;
        (void)memcpy((*destination)->sa_data, (*source)->sa_data, sizeof((*source)->sa_data));

        result = 0;
    }

    return result;
}

void umocktypes_free_const_struct_sockaddr_ptr(struct sockaddr** value)
{
    free(*value);
}

/* after this point malloc is gballoc */
#include "azure_c_shared_utility/gballoc.h"

static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

BEGIN_TEST_SUITE(socketio_win32_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;
    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    result = umocktypes_charptr_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_UMOCK_ALIAS_TYPE(SOCKET_MUTABLE_BUFFER, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SOCKET_CONST_BUFFER, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CONCRETE_IO_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SINGLYLINKEDLIST_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LIST_ITEM_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(OPTIONHANDLER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(pfCloneOption, void*);
    REGISTER_UMOCK_ALIAS_TYPE(pfDestroyOption, void*);
    REGISTER_UMOCK_ALIAS_TYPE(pfSetOption, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SOCKET, void*);
    REGISTER_UMOCK_ALIAS_TYPE(INT, int);
    REGISTER_UMOCK_ALIAS_TYPE(VOID*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(PSTR, void*);
    REGISTER_UMOCK_ALIAS_TYPE(PCSTR, char*);
    REGISTER_UMOCK_ALIAS_TYPE(fd_set*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(const struct timeval*, void*);
    REGISTER_TYPE(const ADDRINFOA*, const_ADDRINFOA_ptr);
    REGISTER_UMOCK_ALIAS_TYPE(PADDRINFOA, const ADDRINFOA*);
    REGISTER_UMOCK_ALIAS_TYPE(DWORD, unsigned long);
    REGISTER_UMOCK_ALIAS_TYPE(LPVOID, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LPDWORD, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LPWSAOVERLAPPED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LPWSAOVERLAPPED_COMPLETION_ROUTINE, void*);

    REGISTER_GLOBAL_MOCK_RETURN(singlylinkedlist_remove, 0);
    REGISTER_GLOBAL_MOCK_RETURN(singlylinkedlist_create, TEST_SINGLYLINKEDLIST_HANDLE);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
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

    currentmalloc_call = 0;
    whenShallmalloc_fail = 0;
    list_head_count = 0;
    singlylinkedlist_add_called = false;
    g_addrinfo_call_fail = false;
    g_addrinfo_two_addresses = false;
    g_addrinfo_dual_stack = false;
    g_last_addrinfo_family = AF_UNSPEC;
    g_last_select_timeout_ms = 0;
    g_v6only_set_count = 0;
    g_v6only_last_value = -1;
    g_connect_attempt_count = 0;
    memset(g_connect_families, 0, sizeof(g_connect_families));
    g_socket_error = 0;
    g_open_result.result = IO_OPEN_CANCELLED;
    g_open_result.code = 0;
    //g_socket_send_size_value = -1;
    g_socket_recv_size_value = -1;
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    TEST_MUTEX_RELEASE(g_testByTest);
}

static void OnSendComplete(void* context, IO_SEND_RESULT send_result)
{
    (void)context;
    (void)send_result;
}

/* socketio_win32_create */
TEST_FUNCTION(socketio_create_io_create_parameters_NULL_fails)
{
    // arrange

    // act
    CONCRETE_IO_HANDLE ioHandle = socketio_create(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(ioHandle);
}

TEST_FUNCTION(socketio_create_singlylinkedlist_create_fails)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(singlylinkedlist_create()).SetReturn((SINGLYLINKEDLIST_HANDLE)NULL);
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    ioHandle = socketio_create(&socketConfig);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(ioHandle);
}

TEST_FUNCTION(socketio_create_succeeds)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(singlylinkedlist_create());
    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));

    // act
    ioHandle = socketio_create(&socketConfig);

    // assert
    ASSERT_IS_NOT_NULL(ioHandle);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

// socketio_win32_destroy 
TEST_FUNCTION(socketio_destroy_socket_io_NULL_succeeds)
{
    // arrange

    // act
    socketio_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

TEST_FUNCTION(socketio_destroy_socket_succeeds)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();
    EXPECTED_CALL(singlylinkedlist_get_head_item(IGNORED_PTR_ARG));
    EXPECTED_CALL(send(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG))
        .SetReturn(0);
    EXPECTED_CALL(WSAGetLastError())
        .SetReturn(WSAEWOULDBLOCK);
    (void)socketio_send(ioHandle, TEST_BUFFER_VALUE, TEST_BUFFER_SIZE, OnSendComplete, (void*)TEST_CALLBACK_CONTEXT);

    umock_c_reset_all_calls();

    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    EXPECTED_CALL(singlylinkedlist_get_head_item(IGNORED_PTR_ARG));
    EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(singlylinkedlist_remove(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(singlylinkedlist_get_head_item(IGNORED_PTR_ARG));
    EXPECTED_CALL(singlylinkedlist_destroy(IGNORED_PTR_ARG));
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    socketio_destroy(ioHandle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

TEST_FUNCTION(socketio_open_socket_io_NULL_fails)
{
    // arrange

    // act
    int result = socketio_open(NULL, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
}

TEST_FUNCTION(socketio_open_ipv6_literal_without_opt_in_requests_ipv4_only)
{
    SOCKETIO_CONFIG socketConfig = { "::1", PORT_NUM, NULL, 0 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    g_addrinfo_call_fail = true;
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();

    (void)socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, AF_INET, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_retrieveoptions_succeeds)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL, 1 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    OPTIONHANDLER_HANDLE options = socketio_get_interface_description()->concrete_io_retrieveoptions(ioHandle);

    ASSERT_ARE_EQUAL(void_ptr, (OPTIONHANDLER_HANDLE)0x4244, options);
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv6_literal_with_opt_in_requests_both_families)
{
    SOCKETIO_CONFIG socketConfig = { "::1", PORT_NUM, NULL, 1 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    g_addrinfo_call_fail = true;
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();

    (void)socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, AF_UNSPEC, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

static void open_and_assert_resolver_family(CONCRETE_IO_HANDLE ioHandle, int expected_family)
{
    int result;

    umock_c_reset_all_calls();
    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, expected_family, g_last_addrinfo_family);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

TEST_FUNCTION(socketio_open_default_ipv6_config_uses_AF_INET)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL, 0 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    open_and_assert_resolver_family(ioHandle, AF_INET);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_false_ipv6_config_uses_AF_INET)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL, 0 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    open_and_assert_resolver_family(ioHandle, AF_INET);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_true_ipv6_config_uses_AF_UNSPEC)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL, 1 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    open_and_assert_resolver_family(ioHandle, AF_UNSPEC);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv6_config_persists_between_separate_opens)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL, 1 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    open_and_assert_resolver_family(ioHandle, AF_UNSPEC);

    umock_c_reset_all_calls();
    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    ASSERT_ARE_EQUAL(int, 0, socketio_close(ioHandle, NULL, NULL));
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    open_and_assert_resolver_family(ioHandle, AF_UNSPEC);

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ipv4_and_localhost_literals_keep_the_default_AF_INET_hint)
{
    const char* hosts[] = { "127.0.0.1", "localhost" };
    size_t i;

    for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++)
    {
        SOCKETIO_CONFIG socketConfig = { hosts[i], PORT_NUM, NULL, 0 };
        CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
        open_and_assert_resolver_family(ioHandle, AF_INET);
        socketio_destroy(ioHandle);
    }
}

/* The opt-in belongs to the IO instance, not to one connection: an IO that is
   closed and opened again, as a WebSocket client does when it reconnects, keeps
   asking the resolver for both families every time. */
TEST_FUNCTION(socketio_open_keeps_the_ipv6_opt_in_across_close_and_reopen)
{
    int cycle;
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL, 1 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    for (cycle = 0; cycle < 4; cycle++)
    {
        g_last_addrinfo_family = -1;
        open_and_assert_resolver_family(ioHandle, AF_UNSPEC);

        umock_c_reset_all_calls();
        EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
        ASSERT_ARE_EQUAL(int, 0, socketio_close(ioHandle, NULL, NULL));
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    socketio_destroy(ioHandle);
}

/* Windows creates an AF_INET6 socket v6-only, and a v6-only socket refuses an
   IPv4-mapped destination such as ::ffff:203.0.113.1 before sending anything.
   Touching the option is not enough: the value handed to setsockopt must be 0. */
TEST_FUNCTION(socketio_open_makes_an_ipv6_candidate_socket_dual_stack)
{
    int result;
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL, 1 };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    g_addrinfo_dual_stack = true;
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    // The first candidate is AF_INET6 and connects at once.
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(setsockopt(IGNORED_NUM_ARG, IPPROTO_IPV6, IPV6_V6ONLY, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);
    ASSERT_ARE_EQUAL(size_t, (size_t)1, g_v6only_set_count);
    ASSERT_ARE_EQUAL(int, 0, g_v6only_last_value);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}
TEST_FUNCTION(socketio_open_socket_fails)
{
    // arrange
    int result;
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG))
        .SetReturn(INVALID_SOCKET);

    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAENOBUFS);
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    // act
    result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, WSAENOBUFS, g_open_result.code);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_getaddrinfo_fails)
{
    // arrange
    int result;
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    umock_c_reset_all_calls();

    g_addrinfo_call_fail = true;
    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();

    // act
    result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_connect_fails)
{
    // arrange
    int result;
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG))
        .SetReturn(SOCKET_ERROR);

    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAECONNREFUSED);

    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    // act
    result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, WSAECONNREFUSED, g_open_result.code);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_ioctlsocket_fails)
{
    // arrange
    int result;
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG))
        .SetReturn(WSAENETDOWN);

    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAENETDOWN);

    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    // act
    result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, WSAENETDOWN, g_open_result.code);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_succeeds)
{
    // arrange
    int result;
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    // act
    result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_pending_connect_succeeds)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    EXPECTED_CALL(select(0, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).SetReturn(1);
    EXPECTED_CALL(getsockopt(IGNORED_NUM_ARG, SOL_SOCKET, SO_ERROR, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(int, 10000, g_last_select_timeout_ms);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_pending_connect_times_out)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    EXPECTED_CALL(select(0, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).SetReturn(0);
    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, WSAETIMEDOUT, g_open_result.code);
    ASSERT_ARE_EQUAL(int, 10000, g_last_select_timeout_ms);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_pending_connect_reports_socket_error)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    g_socket_error = WSAECONNREFUSED;
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    EXPECTED_CALL(select(0, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).SetReturn(1);
    EXPECTED_CALL(getsockopt(IGNORED_NUM_ARG, SOL_SOCKET, SO_ERROR, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, WSAECONNREFUSED, g_open_result.code);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_pending_connect_select_failure_reports_the_wsa_error)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    // select() itself fails (not a timeout): the candidate fails with the WSA
    // error select reported and its socket is released.
    EXPECTED_CALL(select(0, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAENOTSOCK);
    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_ERROR, g_open_result.result);
    ASSERT_ARE_EQUAL(int, WSAENOTSOCK, g_open_result.code);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_open_timeout_falls_back_to_next_address)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    g_addrinfo_two_addresses = true;
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    EXPECTED_CALL(select(0, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).SetReturn(0);
    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(0);
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    /* Both candidates are granted the full per-address timeout; there is no
       shared budget to divide between them. */
    ASSERT_ARE_EQUAL(int, 10000, g_last_select_timeout_ms);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

/* No budget is shared across candidates, so however many blackholed addresses
   of one family come first, the family behind them still gets its attempt. */
TEST_FUNCTION(socketio_open_reaches_ipv4_after_blackholed_ipv6_candidates)
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    g_addrinfo_dual_stack = true;
    umock_c_reset_all_calls();

    EXPECTED_CALL(getaddrinfo(IGNORED_PTR_ARG, IGNORED_PTR_ARG, &TEST_ADDR_INFO, IGNORED_PTR_ARG)).IgnoreArgument_pHints();
    // First IPv6 candidate: blackholed, spends its full grant.
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(setsockopt(IGNORED_NUM_ARG, IPPROTO_IPV6, IPV6_V6ONLY, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    EXPECTED_CALL(select(0, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).SetReturn(0);
    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    // Second IPv6 candidate: also blackholed, and also gets a full grant.
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(setsockopt(IGNORED_NUM_ARG, IPPROTO_IPV6, IPV6_V6ONLY, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(SOCKET_ERROR);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    EXPECTED_CALL(select(0, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).SetReturn(0);
    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));
    // IPv4 candidate: still reached, and connects. No IPV6_V6ONLY call here -
    // the option is only meaningful on an AF_INET6 socket.
    EXPECTED_CALL(socket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(ioctlsocket(IGNORED_NUM_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG));
    EXPECTED_CALL(inet_ntop(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(connect(IGNORED_NUM_ARG, &test_sock_addr, IGNORED_NUM_ARG)).SetReturn(0);
    EXPECTED_CALL(freeaddrinfo(&TEST_ADDR_INFO)).IgnoreArgument_pResult();

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext,
        test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int, IO_OPEN_OK, g_open_result.result);
    ASSERT_ARE_EQUAL(size_t, (size_t)3, g_connect_attempt_count);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[0]);
    ASSERT_ARE_EQUAL(int, AF_INET6, g_connect_families[1]);
    ASSERT_ARE_EQUAL(int, AF_INET, g_connect_families[2]);
    /* Each candidate is granted the full per-address timeout. */
    ASSERT_ARE_EQUAL(int, 10000, g_last_select_timeout_ms);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_close_socket_io_NULL_fails)
{
    // arrange
    // act
    int result = socketio_close(NULL, test_on_io_close_complete, (void*)0x4242);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

TEST_FUNCTION(socketio_close_Succeeds)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    static ADDRINFO addrInfo = { AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP, 128, NULL, (struct sockaddr*)0x11, NULL };

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();

    EXPECTED_CALL(closesocket(IGNORED_NUM_ARG));

    // act
    result = socketio_close(ioHandle, test_on_io_close_complete, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_send_socket_io_fails)
{
    // arrange

    // act
    int result = socketio_send(NULL, (const void*)TEST_BUFFER_VALUE, TEST_BUFFER_SIZE, OnSendComplete, (void*)TEST_CALLBACK_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

TEST_FUNCTION(socketio_send_buffer_NULL_fails)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    static ADDRINFO addrInfo = { AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP, 128, NULL, (struct sockaddr*)0x11, NULL };

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();

    // act
    result = socketio_send(ioHandle, NULL, TEST_BUFFER_SIZE, OnSendComplete, (void*)TEST_CALLBACK_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_send_size_zero_fails)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    static ADDRINFO addrInfo = { AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP, 128, NULL, (struct sockaddr*)0x11, NULL };

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();

    // act
    result = socketio_send(ioHandle, (const void*)TEST_BUFFER_VALUE, 0, OnSendComplete, (void*)TEST_CALLBACK_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_send_succeeds)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    static ADDRINFO addrInfo = { AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP, 128, NULL, (struct sockaddr*)0x11, NULL };

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();

    EXPECTED_CALL(singlylinkedlist_get_head_item(IGNORED_PTR_ARG));
    EXPECTED_CALL(send(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));

    // act
    result = socketio_send(ioHandle, (const void*)TEST_BUFFER_VALUE, TEST_BUFFER_SIZE, OnSendComplete, (void*)TEST_CALLBACK_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_send_returns_1_succeeds)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    static ADDRINFO addrInfo = { AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP, 128, NULL, (struct sockaddr*)0x11, NULL };

    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();

    EXPECTED_CALL(singlylinkedlist_get_head_item(IGNORED_PTR_ARG));
    EXPECTED_CALL(send(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG)).SetReturn(1);
    EXPECTED_CALL(WSAGetLastError()).SetReturn(WSAEWOULDBLOCK);
    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    EXPECTED_CALL(singlylinkedlist_add(IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = socketio_send(ioHandle, (const void*)TEST_BUFFER_VALUE, TEST_BUFFER_SIZE, OnSendComplete, (void*)TEST_CALLBACK_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    list_head_count = list_item_count;
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_dowork_socket_io_NULL_fails)
{
    // arrange

    // act
    socketio_dowork(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

TEST_FUNCTION(socketio_dowork_succeeds)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    (void)socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();

    EXPECTED_CALL(singlylinkedlist_get_head_item(IGNORED_PTR_ARG));
    EXPECTED_CALL(recv(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(WSAGetLastError());

    // act
    socketio_dowork(ioHandle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_dowork_recv_bytes_succeeds)
{
    // arrange
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);

    (void)socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);

    umock_c_reset_all_calls();

    EXPECTED_CALL(singlylinkedlist_get_head_item(IGNORED_PTR_ARG));
    EXPECTED_CALL(recv(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG))
        .CopyOutArgumentBuffer(2, "t", 1)
        .SetReturn(1);
    EXPECTED_CALL(recv(IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_NUM_ARG));
    EXPECTED_CALL(WSAGetLastError());

    // act
    socketio_dowork(ioHandle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    socketio_destroy(ioHandle);
}

// socketio_setoption tests

static CONCRETE_IO_HANDLE setup_socket()
{
    SOCKETIO_CONFIG socketConfig = { HOSTNAME_ARG, PORT_NUM, NULL };
    CONCRETE_IO_HANDLE ioHandle = socketio_create(&socketConfig);
    int result = socketio_open(ioHandle, test_on_io_open_complete, &callbackContext, test_on_bytes_received, &callbackContext, test_on_io_error, &callbackContext);
    ASSERT_ARE_EQUAL(int, 0, result);
    return ioHandle;
}

static CONCRETE_IO_HANDLE setup_socket_and_expect_WSAIoctl()
{
    CONCRETE_IO_HANDLE ioHandle = setup_socket();

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(WSAIoctl(*(SOCKET*)ioHandle, SIO_KEEPALIVE_VALS, IGNORED_PTR_ARG,
        sizeof(struct tcp_keepalive), NULL, 0, IGNORED_PTR_ARG, NULL, NULL))
        .IgnoreArgument_lpvInBuffer()
        .IgnoreArgument_lpcbBytesReturned();

    memset(&persisted_tcp_keepalive, 0, sizeof(struct tcp_keepalive));

    return ioHandle;
}

static void verify_mocks_and_destroy_socket(CONCRETE_IO_HANDLE ioHandle)
{
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    socketio_destroy(ioHandle);
}

TEST_FUNCTION(socketio_setoption_fails_when_handle_is_null)
{
    // arrange
    int irrelevant = 1;

    // act
    int result = socketio_setoption(NULL, "tcp_keepalive", &irrelevant);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

TEST_FUNCTION(socketio_setoption_fails_when_option_name_is_null)
{
    // arrange
    int irrelevant = 1;
    int result;

    CONCRETE_IO_HANDLE ioHandle = setup_socket();

    umock_c_reset_all_calls();

    // act
    result = socketio_setoption(ioHandle, NULL, &irrelevant);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(socketio_setoption_fails_when_value_is_null)
{
    // arrange
    int result;
    CONCRETE_IO_HANDLE ioHandle = setup_socket();

    umock_c_reset_all_calls();

    // act
    result = socketio_setoption(ioHandle, "tcp_keepalive", NULL);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(socketio_setoption_fails_when_it_receives_an_unsupported_option)
{
    // arrange
    int irrelevant = 1;
    int result;

    CONCRETE_IO_HANDLE ioHandle = setup_socket();

    umock_c_reset_all_calls();

    // act
    result = socketio_setoption(ioHandle, "unsupported_option_name", &irrelevant);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(calling_socketio_setoption_with_tcp_keepalive_does_not_impact_the_other_two_options)
{
    // arrange
    int irrelevant = 1;
    CONCRETE_IO_HANDLE ioHandle = setup_socket_and_expect_WSAIoctl();

    // act
    int result = socketio_setoption(ioHandle, "tcp_keepalive", &irrelevant);
    ASSERT_ARE_EQUAL(int, 0, result);

    // assert
    ASSERT_ARE_EQUAL(int, 0, persisted_tcp_keepalive.keepalivetime);
    ASSERT_ARE_EQUAL(int, 0, persisted_tcp_keepalive.keepaliveinterval);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(calling_socketio_setoption_for_option_tcp_keepalive_time_does_not_impact_the_other_two)
{
    // arrange
    int irrelevant = 1;
    CONCRETE_IO_HANDLE ioHandle = setup_socket_and_expect_WSAIoctl();

    // act
    int result = socketio_setoption(ioHandle, "tcp_keepalive_time", &irrelevant);
    ASSERT_ARE_EQUAL(int, 0, result);

    // assert
    ASSERT_ARE_EQUAL(int, 0, persisted_tcp_keepalive.onoff);
    ASSERT_ARE_EQUAL(int, 0, persisted_tcp_keepalive.keepaliveinterval);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(calling_socketio_setoption_for_option_tcp_keepalive_interval_does_not_impact_the_other_two)
{
    // arrange
    int irrelevant = 1;
    CONCRETE_IO_HANDLE ioHandle = setup_socket_and_expect_WSAIoctl();

    // act
    int result = socketio_setoption(ioHandle, "tcp_keepalive_interval", &irrelevant);
    ASSERT_ARE_EQUAL(int, 0, result);

    // assert
    ASSERT_ARE_EQUAL(int, 0, persisted_tcp_keepalive.onoff);
    ASSERT_ARE_EQUAL(int, 0, persisted_tcp_keepalive.keepalivetime);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(tcp_keepalive_time_arg_to_socketio_setoption_is_converted_to_milliseconds)
{
    // arrange
    int result;
    CONCRETE_IO_HANDLE ioHandle = setup_socket_and_expect_WSAIoctl();

    int time = 3;

    // act
    result = socketio_setoption(ioHandle, "tcp_keepalive_time", &time);
    ASSERT_ARE_EQUAL(int, 0, result);

    // assert
    ASSERT_ARE_EQUAL(int, time * 1000, persisted_tcp_keepalive.keepalivetime);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(tcp_keepalive_interval_arg_to_socketio_setoption_is_converted_to_milliseconds)
{
    // arrange
    int result;
    CONCRETE_IO_HANDLE ioHandle = setup_socket_and_expect_WSAIoctl();

    int interval = 15;

    // act
    result = socketio_setoption(ioHandle, "tcp_keepalive_interval", &interval);
    ASSERT_ARE_EQUAL(int, 0, result);

    // assert
    ASSERT_ARE_EQUAL(int, interval * 1000, persisted_tcp_keepalive.keepaliveinterval);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(tcp_keepalive_arg_is_not_modified_by_socketio_setoption)
{
    // arrange
    int result;
    CONCRETE_IO_HANDLE ioHandle = setup_socket_and_expect_WSAIoctl();

    int onoff = -42;

    // act
    result = socketio_setoption(ioHandle, "tcp_keepalive", &onoff);
    ASSERT_ARE_EQUAL(int, 0, result);

    // assert
    ASSERT_ARE_EQUAL(int, onoff, persisted_tcp_keepalive.onoff);

    verify_mocks_and_destroy_socket(ioHandle);
}

TEST_FUNCTION(socketio_setoption_does_not_persist_keepalive_values_if_WSAIoctl_fails)
{
    // arrange
    int irrelevant = 1;
    int result;

    CONCRETE_IO_HANDLE ioHandle = setup_socket();

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(WSAIoctl(*(SOCKET*)ioHandle, SIO_KEEPALIVE_VALS, IGNORED_PTR_ARG,
        sizeof(struct tcp_keepalive), NULL, 0, IGNORED_PTR_ARG, NULL, NULL))
        .IgnoreArgument_lpvInBuffer()
        .IgnoreArgument_lpcbBytesReturned()
        .SetReturn(1); // 1st call fails, keepalive changes should be discarded
    STRICT_EXPECTED_CALL(WSAIoctl(*(SOCKET*)ioHandle, SIO_KEEPALIVE_VALS, IGNORED_PTR_ARG,
        sizeof(struct tcp_keepalive), NULL, 0, IGNORED_PTR_ARG, NULL, NULL))
        .IgnoreArgument_lpvInBuffer()
        .IgnoreArgument_lpcbBytesReturned()
        .SetReturn(0); // purpose of 2nd call is just to see the keepalive state after the 1st call

    memset(&persisted_tcp_keepalive, 0, sizeof(struct tcp_keepalive));

    // act
    result = socketio_setoption(ioHandle, "tcp_keepalive", &irrelevant);
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    result = socketio_setoption(ioHandle, "tcp_keepalive_time", &irrelevant); // use different option for 2nd call so we don't overwrite the value from the 1st
    ASSERT_ARE_EQUAL(int, 0, result);

    // assert
    ASSERT_ARE_EQUAL(int, 0, persisted_tcp_keepalive.onoff);

    verify_mocks_and_destroy_socket(ioHandle);
}

END_TEST_SUITE(socketio_win32_unittests)
