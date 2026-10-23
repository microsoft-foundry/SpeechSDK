// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/pem.h>

#include "testrunnerswitcher.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/tlsio_openssl.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/threadapi.h"
#include "tls_test_server.h"

typedef struct OPEN_RESULT_TAG
{
    int completed;
    IO_OPEN_RESULT result;
} OPEN_RESULT;

static TLS_TEST_CA* test_ca;

static void open_complete(void* context, IO_OPEN_RESULT_DETAILED result)
{
    OPEN_RESULT* state = (OPEN_RESULT*)context;
    state->completed = 1;
    state->result = result.result;
}

static int handshake_result(CONCRETE_IO_HANDLE io, OPEN_RESULT* state)
{
    time_t deadline = time(NULL) + 8;
    memset(state, 0, sizeof(*state));
    if (tlsio_openssl_open(io, open_complete, state, NULL, NULL, NULL, NULL) != 0)
    {
        return 0;
    }
    while (!state->completed && time(NULL) < deadline)
    {
        tlsio_openssl_dowork(io);
        ThreadAPI_Sleep(1);
    }
    return state->completed;
}

static int run_handshake(const char* hostname, const char* server_san,
    TLS_TEST_CA* trusted_ca, IO_OPEN_RESULT expected)
{
    int ok = 0;
    int accepted;
    int crl_served = 0;
    char* trusted_ca_pem = tls_test_ca_pem(trusted_ca);
    TLS_TEST_SERVER* server = NULL;
    CONCRETE_IO_HANDLE io = NULL;
    TLSIO_CONFIG config = { 0 };
    OPEN_RESULT outcome;
    const bool isolate_trust = true;
    unsigned char ip_san[16];
    const unsigned char expected_last_byte =
        strcmp(server_san, "IP:::2") == 0 ? 2 : 1;

    if (trusted_ca_pem == NULL ||
        (server = tls_test_server_start(test_ca, server_san)) == NULL)
    {
        goto cleanup;
    }
    if (tls_test_server_leaf_ip_san(server, ip_san, sizeof(ip_san)) != sizeof(ip_san)
        || memcmp(ip_san, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 15) != 0
        || ip_san[15] != expected_last_byte)
    {
        goto cleanup;
    }
    config.hostname = hostname;
    config.port = tls_test_server_port(server);
    config.enable_ipv6 = 1;
    io = tlsio_openssl_create(&config);
    if (io == NULL ||
        tlsio_openssl_setoption(io, OPTION_DISABLE_DEFAULT_VERIFY_PATHS, &isolate_trust) != 0 ||
        tlsio_openssl_setoption(io, OPTION_TRUSTED_CERT, trusted_ca_pem) != 0)
    {
        goto cleanup;
    }
    ok = handshake_result(io, &outcome) && outcome.result == expected;

cleanup:
    if (io != NULL)
    {
        tlsio_openssl_destroy(io);
    }
    accepted = server == NULL ? 0 : tls_test_server_stop(server, &crl_served);
    free(trusted_ca_pem);
    return ok && accepted == 1 && (expected != IO_OPEN_OK || crl_served);
}

BEGIN_TEST_SUITE(tlsio_openssl_ut)

TEST_SUITE_INITIALIZE(initialize_ca)
{
    test_ca = tls_test_ca_create();
    ASSERT_IS_NOT_NULL(test_ca);
}

TEST_SUITE_CLEANUP(cleanup_ca)
{
    tls_test_ca_destroy(test_ca);
}

TEST_FUNCTION(create_rejects_null_hostname)
{
    TLSIO_CONFIG config = { 0 };
    config.port = 443;

    ASSERT_IS_NULL(tlsio_openssl_create(&config));
}

TEST_FUNCTION(runtime_ca_der_and_pem_represent_the_same_public_certificate)
{
    int der_size = 0;
    unsigned char* der = tls_test_ca_der(test_ca, &der_size);
    char* pem = tls_test_ca_pem(test_ca);
    const unsigned char* cursor = der;
    BIO* memory = pem == NULL ? NULL : BIO_new_mem_buf(pem, -1);
    X509* from_der = der == NULL ? NULL : d2i_X509(NULL, &cursor, der_size);
    X509* from_pem = memory == NULL ? NULL : PEM_read_bio_X509(memory, NULL, NULL, NULL);
    int same = from_der != NULL && from_pem != NULL &&
        X509_cmp(from_der, from_pem) == 0;

    X509_free(from_der);
    X509_free(from_pem);
    BIO_free(memory);
    free(der);
    free(pem);
    ASSERT_IS_TRUE(same);
}

TEST_FUNCTION(trusted_ca_and_matching_ipv6_ip_san_complete_handshake)
{
    ASSERT_IS_TRUE(run_handshake("::1", "IP:::1", test_ca, IO_OPEN_OK));
}

TEST_FUNCTION(trusted_ca_and_wrong_ipv6_ip_san_reject_handshake)
{
    ASSERT_IS_TRUE(run_handshake("::1", "IP:::2", test_ca, IO_OPEN_ERROR));
}

TEST_FUNCTION(matching_ipv6_ip_san_from_untrusted_ca_rejects_handshake)
{
    TLS_TEST_CA* unrelated_ca = tls_test_ca_create();
    int rejected = unrelated_ca != NULL &&
        run_handshake("::1", "IP:::1", unrelated_ca, IO_OPEN_ERROR);

    tls_test_ca_destroy(unrelated_ca);
    ASSERT_IS_TRUE(rejected);
}

TEST_FUNCTION(scoped_ipv6_loopback_matches_unscoped_ip_san)
{
    ASSERT_IS_TRUE(run_handshake("::1%1", "IP:::1", test_ca, IO_OPEN_OK));
}

END_TEST_SUITE(tlsio_openssl_ut)
