// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <wincrypt.h>

#include "testrunnerswitcher.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/tlsio_schannel.h"
#include "tls_test_server.h"

typedef struct OPEN_RESULT_TAG
{
    int completed;
    IO_OPEN_RESULT result;
    int code;
} OPEN_RESULT;

static TLS_TEST_CA* test_ca;
static HCERTSTORE root_store;
static PCCERT_CONTEXT installed_ca;

static int remove_test_ca(void)
{
    int removed = 1;
    if (installed_ca != NULL)
    {
        removed = CertDeleteCertificateFromStore(installed_ca) == TRUE;
        installed_ca = NULL;
    }
    if (root_store != NULL)
    {
        removed = CertCloseStore(root_store, CERT_CLOSE_STORE_CHECK_FLAG) == TRUE && removed;
        root_store = NULL;
    }
    return removed;
}

static int install_test_ca(void)
{
    int der_size = 0;
    unsigned char* der = tls_test_ca_der(test_ca, &der_size);
    PCCERT_CONTEXT certificate = der == NULL ? NULL :
        CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, der, (DWORD)der_size);
    int result = 0;

    if (certificate != NULL)
    {
        root_store = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0,
            CERT_SYSTEM_STORE_CURRENT_USER, "Root");
        if (root_store != NULL)
        {
            result = CertAddCertificateContextToStore(root_store, certificate,
                CERT_STORE_ADD_NEW, &installed_ca) == TRUE;
        }
    }
    CertFreeCertificateContext(certificate);
    free(der);
    return result;
}

static void open_complete(void* context, IO_OPEN_RESULT_DETAILED result)
{
    OPEN_RESULT* state = (OPEN_RESULT*)context;
    state->completed = 1;
    state->result = result.result;
    state->code = result.code;
}

static int run_handshake(TLS_TEST_CA* issuing_ca, const char* hostname, const char* server_san,
    int scoped, IO_OPEN_RESULT expected, int expected_code)
{
    TLS_TEST_SERVER* server = tls_test_server_start(issuing_ca, server_san);
    TLSIO_CONFIG config = { 0 };
    SOCKETIO_CONFIG socket_config = { 0 };
    CONCRETE_IO_HANDLE io = NULL;
    OPEN_RESULT outcome = { 0 };
    time_t deadline = time(NULL) + 8;
    int opened = 0;
    int accepted;
    unsigned char ip_san[16];
    const unsigned char expected_last_byte =
        strcmp(server_san, "IP:::2") == 0 ? 2 : 1;

    if (server == NULL)
    {
        return 0;
    }
    if (tls_test_server_leaf_ip_san(server, ip_san, sizeof(ip_san)) != sizeof(ip_san)
        || memcmp(ip_san, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 15) != 0
        || ip_san[15] != expected_last_byte)
    {
        (void)tls_test_server_stop(server, NULL);
        return 0;
    }
    config.hostname = hostname;
    config.port = tls_test_server_port(server);
    config.enable_ipv6 = 1;
    if (scoped)
    {
        // Exercise the scoped TLS identity independently of platform-specific scoped loopback routing.
        socket_config.hostname = "::1";
        socket_config.port = config.port;
        socket_config.enable_ipv6 = 1;
        config.underlying_io_interface = socketio_get_interface_description();
        config.underlying_io_parameters = &socket_config;
    }
    io = tlsio_schannel_create(&config);
    if (io != NULL && tlsio_schannel_open(io, open_complete, &outcome, NULL, NULL, NULL, NULL) == 0)
    {
        while (!outcome.completed && time(NULL) < deadline)
        {
            tlsio_schannel_dowork(io);
            ThreadAPI_Sleep(1);
        }
        opened = outcome.completed
            && outcome.result == expected
            && (expected == IO_OPEN_OK || outcome.code == expected_code);
    }
    if (io != NULL)
    {
        tlsio_schannel_destroy(io);
    }
    accepted = tls_test_server_stop(server, NULL);
    return opened && accepted == 1;
}

BEGIN_TEST_SUITE(tlsio_schannel_ipv6_ut)

TEST_SUITE_INITIALIZE(initialize_ca)
{
    int installed;
    int removed = 1;
    test_ca = tls_test_ca_create();
    installed = test_ca != NULL && install_test_ca();
    if (!installed)
    {
        removed = remove_test_ca();
        tls_test_ca_destroy(test_ca);
        test_ca = NULL;
    }
    ASSERT_IS_TRUE(removed);
    ASSERT_IS_TRUE(installed);
}

TEST_SUITE_CLEANUP(cleanup_ca)
{
    int removed = remove_test_ca();
    tls_test_ca_destroy(test_ca);
    test_ca = NULL;
    ASSERT_IS_TRUE(removed);
}

TEST_FUNCTION(trusted_ca_and_matching_ipv6_ip_san_complete_schannel_handshake)
{
    ASSERT_IS_TRUE(run_handshake(test_ca, "::1", "IP:::1", 0, IO_OPEN_OK, 0));
}

TEST_FUNCTION(trusted_ca_and_wrong_ipv6_ip_san_reject_schannel_handshake)
{
    ASSERT_IS_TRUE(run_handshake(
        test_ca, "::1", "IP:::2", 0, IO_OPEN_ERROR, SEC_E_WRONG_PRINCIPAL));
}

TEST_FUNCTION(untrusted_ca_and_matching_ipv6_ip_san_reject_schannel_handshake)
{
    TLS_TEST_CA* unrelated_ca = tls_test_ca_create();
    int rejected = unrelated_ca != NULL &&
        run_handshake(
            unrelated_ca, "::1", "IP:::1", 0, IO_OPEN_ERROR, SEC_E_UNTRUSTED_ROOT);

    tls_test_ca_destroy(unrelated_ca);
    ASSERT_IS_TRUE(rejected);
}

TEST_FUNCTION(scoped_ipv6_tls_identity_matches_unscoped_ip_san)
{
    ASSERT_IS_TRUE(run_handshake(test_ca, "::1%1", "IP:::1", 1, IO_OPEN_OK, 0));
}

END_TEST_SUITE(tlsio_schannel_ipv6_ut)
