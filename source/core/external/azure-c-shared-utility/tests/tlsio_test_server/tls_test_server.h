// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef TLS_TEST_SERVER_H
#define TLS_TEST_SERVER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TLS_TEST_CA_TAG TLS_TEST_CA;
typedef struct TLS_TEST_SERVER_TAG TLS_TEST_SERVER;

// The CA and server private keys never leave this test fixture or touch disk.
TLS_TEST_CA* tls_test_ca_create(void);
void tls_test_ca_destroy(TLS_TEST_CA* ca);

// Caller frees the returned public certificate with free(). DER is suitable
// for a disposable Windows runner's temporary CurrentUser Root installation.
char* tls_test_ca_pem(const TLS_TEST_CA* ca);
unsigned char* tls_test_ca_der(const TLS_TEST_CA* ca, int* length);

// Binds ::1 for one TLS connection and serves a CA-signed, empty CRL from
// memory via a separate ephemeral 127.0.0.1 HTTP endpoint. The SAN is an
// OpenSSL X509V3 value such as "IP:::1" or "IP:::2".
TLS_TEST_SERVER* tls_test_server_start(TLS_TEST_CA* ca, const char* san);
int tls_test_server_port(const TLS_TEST_SERVER* server);
size_t tls_test_server_leaf_ip_san(
    const TLS_TEST_SERVER* server, unsigned char* address, size_t address_size);

// Joins both threads and reports whether the CRL was served from memory.
// Returns 1 if TLS was accepted, 0 if none arrived, or -1 on join failure.
// Frees the server after successful joins; a failed join cannot free live state.
int tls_test_server_stop(TLS_TEST_SERVER* server, int* crl_served);

#ifdef __cplusplus
}
#endif

#endif
