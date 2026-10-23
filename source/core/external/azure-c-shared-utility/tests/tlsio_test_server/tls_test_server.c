// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

#include "azure_c_shared_utility/threadapi.h"
#include "tls_test_server.h"

#ifdef _WIN32
typedef SOCKET TEST_SOCKET;
#define INVALID_TEST_SOCKET INVALID_SOCKET
#define close_test_socket closesocket
#else
typedef int TEST_SOCKET;
#define INVALID_TEST_SOCKET (-1)
#define close_test_socket close
#endif

struct TLS_TEST_CA_TAG
{
    EVP_PKEY* key;
    X509* certificate;
    long next_serial;
};

struct TLS_TEST_SERVER_TAG
{
    TEST_SOCKET listener;
    TEST_SOCKET crl_listener;
    SSL_CTX* context;
    THREAD_HANDLE thread;
    THREAD_HANDLE crl_thread;
    unsigned char* crl_der;
    int crl_length;
    int crl_port;
    int port;
    int accepted;
    int crl_served;
    unsigned char leaf_ip_san[16];
    size_t leaf_ip_san_length;
#ifdef _WIN32
    int winsock_started;
#endif
};

static EVP_PKEY* generate_key(void)
{
    EVP_PKEY* key = NULL;
    EVP_PKEY_CTX* context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (context != NULL)
    {
        if (EVP_PKEY_keygen_init(context) != 1 ||
            EVP_PKEY_CTX_set_rsa_keygen_bits(context, 2048) != 1 ||
            EVP_PKEY_keygen(context, &key) != 1)
        {
            EVP_PKEY_free(key);
            key = NULL;
        }
        EVP_PKEY_CTX_free(context);
    }
    return key;
}

static int add_extension(X509* certificate, X509* issuer, int nid, const char* value)
{
    X509V3_CTX context;
    X509_EXTENSION* extension;
    int result = 0;

    X509V3_set_ctx(&context, issuer, certificate, NULL, NULL, 0);
    extension = X509V3_EXT_conf_nid(NULL, &context, nid, (char*)value);
    if (extension != NULL)
    {
        result = X509_add_ext(certificate, extension, -1);
        X509_EXTENSION_free(extension);
    }
    return result == 1;
}

static X509* generate_certificate(EVP_PKEY* key, X509* issuer, EVP_PKEY* issuer_key,
    const char* san, const char* crl_url, const char* subject_name, long serial)
{
    X509* certificate = X509_new();
    X509_NAME* subject;
    int is_ca = (issuer == NULL);

    if (certificate == NULL || X509_set_version(certificate, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(certificate), serial) != 1 ||
        X509_gmtime_adj(X509_get_notBefore(certificate), -60) == NULL ||
        X509_gmtime_adj(X509_get_notAfter(certificate), 3600) == NULL ||
        X509_set_pubkey(certificate, key) != 1)
    {
        goto error;
    }
    subject = X509_get_subject_name(certificate);
    if (X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
        (const unsigned char*)(is_ca ? subject_name : "test server"), -1, -1, 0) != 1 ||
        X509_set_issuer_name(certificate, is_ca ? subject : X509_get_subject_name(issuer)) != 1 ||
        !add_extension(certificate, is_ca ? certificate : issuer, NID_basic_constraints,
            is_ca ? "critical,CA:TRUE" : "critical,CA:FALSE") ||
        !add_extension(certificate, is_ca ? certificate : issuer, NID_key_usage,
            is_ca ? "critical,keyCertSign,cRLSign" : "critical,digitalSignature,keyEncipherment") ||
        (!is_ca && (!add_extension(certificate, issuer, NID_ext_key_usage, "serverAuth") ||
                    !add_extension(certificate, issuer, NID_subject_alt_name, san) ||
                    !add_extension(certificate, issuer, NID_crl_distribution_points, crl_url))) ||
        X509_sign(certificate, is_ca ? key : issuer_key, EVP_sha256()) <= 0)
    {
        goto error;
    }
    return certificate;

error:
    X509_free(certificate);
    return NULL;
}

TLS_TEST_CA* tls_test_ca_create(void)
{
    TLS_TEST_CA* ca = (TLS_TEST_CA*)calloc(1, sizeof(*ca));
    unsigned char id[8];
    char subject_name[40];
    if (ca != NULL)
    {
        ca->key = generate_key();
        if (ca->key == NULL || RAND_bytes(id, sizeof(id)) != 1 ||
            snprintf(subject_name, sizeof(subject_name), "isolated CA %02x%02x%02x%02x%02x%02x%02x%02x",
                id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]) <= 0 ||
            (ca->certificate = generate_certificate(ca->key, NULL, NULL, NULL, NULL, subject_name, 1)) == NULL)
        {
            tls_test_ca_destroy(ca);
            ca = NULL;
        }
        else
        {
            ca->next_serial = 2;
        }
    }
    return ca;
}

void tls_test_ca_destroy(TLS_TEST_CA* ca)
{
    if (ca != NULL)
    {
        X509_free(ca->certificate);
        EVP_PKEY_free(ca->key);
        free(ca);
    }
}

char* tls_test_ca_pem(const TLS_TEST_CA* ca)
{
    BIO* memory;
    BUF_MEM* buffer;
    char* result = NULL;

    if (ca == NULL || (memory = BIO_new(BIO_s_mem())) == NULL)
    {
        return NULL;
    }
    if (PEM_write_bio_X509(memory, ca->certificate) == 1)
    {
        BIO_get_mem_ptr(memory, &buffer);
        result = (char*)malloc(buffer->length + 1);
        if (result != NULL)
        {
            memcpy(result, buffer->data, buffer->length);
            result[buffer->length] = '\0';
        }
    }
    BIO_free(memory);
    return result;
}

unsigned char* tls_test_ca_der(const TLS_TEST_CA* ca, int* length)
{
    unsigned char* result;
    unsigned char* cursor;
    int size;

    if (ca == NULL || length == NULL || (size = i2d_X509(ca->certificate, NULL)) <= 0)
    {
        return NULL;
    }
    result = (unsigned char*)malloc((size_t)size);
    if (result != NULL)
    {
        cursor = result;
        if (i2d_X509(ca->certificate, &cursor) != size)
        {
            free(result);
            result = NULL;
        }
        else
        {
            *length = size;
        }
    }
    return result;
}

static int has_valid_ca_chain(X509* certificate, const TLS_TEST_CA* ca)
{
    int result = 0;
    X509_STORE* store = X509_STORE_new();
    X509_STORE_CTX* verification = X509_STORE_CTX_new();

    if (store != NULL && verification != NULL &&
        X509_STORE_add_cert(store, ca->certificate) == 1 &&
        X509_STORE_CTX_init(verification, store, certificate, NULL) == 1)
    {
        result = X509_verify_cert(verification) == 1;
    }

    X509_STORE_CTX_free(verification);
    X509_STORE_free(store);
    return result;
}

static int capture_leaf_ip_san(TLS_TEST_SERVER* server, X509* certificate)
{
    GENERAL_NAMES* names = X509_get_ext_d2i(
        certificate, NID_subject_alt_name, NULL, NULL);
    int result = 0;

    if (names != NULL)
    {
        int index;
        int count = sk_GENERAL_NAME_num(names);
        for (index = 0; index < count; index++)
        {
            GENERAL_NAME* name = sk_GENERAL_NAME_value(names, index);
            if (name->type == GEN_IPADD
                && server->leaf_ip_san_length == 0
                && ASN1_STRING_length(name->d.iPAddress) == sizeof(server->leaf_ip_san))
            {
                memcpy(server->leaf_ip_san,
                    ASN1_STRING_get0_data(name->d.iPAddress),
                    sizeof(server->leaf_ip_san));
                server->leaf_ip_san_length = sizeof(server->leaf_ip_san);
            }
            else
            {
                server->leaf_ip_san_length = 0;
                break;
            }
        }
        result = count == 1 && server->leaf_ip_san_length == sizeof(server->leaf_ip_san);
    }
    GENERAL_NAMES_free(names);
    return result;
}

static int make_crl(TLS_TEST_SERVER* server, const TLS_TEST_CA* ca)
{
    X509_CRL* crl = X509_CRL_new();
    ASN1_TIME* last_update = ASN1_TIME_set(NULL, time(NULL) - 60);
    ASN1_TIME* next_update = ASN1_TIME_set(NULL, time(NULL) + 3600);
    unsigned char* cursor;
    int result = 0;

    if (crl != NULL && last_update != NULL && next_update != NULL &&
        X509_CRL_set_version(crl, 1) == 1 &&
        X509_CRL_set_issuer_name(crl, X509_get_subject_name(ca->certificate)) == 1 &&
        X509_CRL_set1_lastUpdate(crl, last_update) == 1 &&
        X509_CRL_set1_nextUpdate(crl, next_update) == 1 &&
        X509_CRL_sign(crl, ca->key, EVP_sha256()) > 0 &&
        (server->crl_length = i2d_X509_CRL(crl, NULL)) > 0 &&
        (server->crl_der = (unsigned char*)malloc((size_t)server->crl_length)) != NULL)
    {
        cursor = server->crl_der;
        result = i2d_X509_CRL(crl, &cursor) == server->crl_length;
    }
    ASN1_TIME_free(next_update);
    ASN1_TIME_free(last_update);
    X509_CRL_free(crl);
    return result;
}

static int send_all(TEST_SOCKET connection, const unsigned char* bytes, size_t length)
{
    while (length > 0)
    {
        int sent = send(connection, (const char*)bytes, (int)length, 0);
        if (sent <= 0)
        {
            return 0;
        }
        bytes += sent;
        length -= (size_t)sent;
    }
    return 1;
}

static int crl_thread(void* context)
{
    TLS_TEST_SERVER* server = (TLS_TEST_SERVER*)context;
    struct timeval timeout = { 5, 0 };
    fd_set readable;
    TEST_SOCKET connection = INVALID_TEST_SOCKET;
    char request[9];
    char response[160];
    int response_size;
    int received = 0;

    FD_ZERO(&readable);
    FD_SET(server->crl_listener, &readable);
    if (select((int)server->crl_listener + 1, &readable, NULL, NULL, &timeout) == 1)
    {
        connection = accept(server->crl_listener, NULL, NULL);
        if (connection != INVALID_TEST_SOCKET)
        {
#ifdef _WIN32
            DWORD milliseconds = 5000;
            (void)setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (const char*)&milliseconds, sizeof(milliseconds));
#else
            (void)setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
            while (received < (int)sizeof(request))
            {
                int chunk = recv(connection, request + received, (int)sizeof(request) - received, 0);
                if (chunk <= 0)
                {
                    break;
                }
                received += chunk;
            }
            if (received == (int)sizeof(request) &&
                memcmp(request, "GET /crl ", sizeof(request)) == 0 &&
                (response_size = snprintf(response, sizeof(response),
                    "HTTP/1.0 200 OK\r\nContent-Type: application/pkix-crl\r\nContent-Length: %d\r\n\r\n",
                    server->crl_length)) > 0 &&
                (size_t)response_size < sizeof(response))
            {
                server->crl_served =
                    send_all(connection, (const unsigned char*)response, (size_t)response_size) &&
                    send_all(connection, server->crl_der, (size_t)server->crl_length);
            }
            (void)close_test_socket(connection);
        }
    }
    return 0;
}

static int server_thread(void* context)
{
    TLS_TEST_SERVER* server = (TLS_TEST_SERVER*)context;
    struct timeval timeout = { 5, 0 };
    fd_set readable;
    TEST_SOCKET connection = INVALID_TEST_SOCKET;
    SSL* ssl = NULL;

    FD_ZERO(&readable);
    FD_SET(server->listener, &readable);
    if (select((int)server->listener + 1, &readable, NULL, NULL, &timeout) == 1)
    {
        connection = accept(server->listener, NULL, NULL);
        if (connection != INVALID_TEST_SOCKET)
        {
            server->accepted = 1;
#ifdef _WIN32
            {
                DWORD milliseconds = 5000;
                (void)setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (const char*)&milliseconds, sizeof(milliseconds));
            }
#else
            (void)setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
            ssl = SSL_new(server->context);
            if (ssl != NULL && SSL_set_fd(ssl, (int)connection) == 1)
            {
                (void)SSL_accept(ssl);
            }
        }
    }
    SSL_free(ssl);
    if (connection != INVALID_TEST_SOCKET)
    {
        (void)close_test_socket(connection);
    }
    return 0;
}

TLS_TEST_SERVER* tls_test_server_start(TLS_TEST_CA* ca, const char* san)
{
    TLS_TEST_SERVER* server;
    EVP_PKEY* server_key = NULL;
    X509* leaf = NULL;
    struct sockaddr_in6 address;
    struct sockaddr_in crl_address;
    char crl_url[96];
#ifdef _WIN32
    int address_size = sizeof(address);
    int crl_address_size = sizeof(crl_address);
    WSADATA winsock;
#else
    socklen_t address_size = sizeof(address);
    socklen_t crl_address_size = sizeof(crl_address);
#endif
    int ipv6_only = 1;

    if (ca == NULL || san == NULL || (server = (TLS_TEST_SERVER*)calloc(1, sizeof(*server))) == NULL)
    {
        return NULL;
    }
    server->listener = INVALID_TEST_SOCKET;
    server->crl_listener = INVALID_TEST_SOCKET;
#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    {
        goto error;
    }
    server->winsock_started = 1;
#endif
    if (!make_crl(server, ca))
    {
        goto error;
    }
    server->crl_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->crl_listener == INVALID_TEST_SOCKET)
    {
        goto error;
    }
    memset(&crl_address, 0, sizeof(crl_address));
    crl_address.sin_family = AF_INET;
    crl_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server->crl_listener, (const struct sockaddr*)&crl_address, sizeof(crl_address)) != 0 ||
        getsockname(server->crl_listener, (struct sockaddr*)&crl_address, &crl_address_size) != 0 ||
        listen(server->crl_listener, 1) != 0 ||
        snprintf(crl_url, sizeof(crl_url), "URI:http://127.0.0.1:%u/crl",
            (unsigned int)ntohs(crl_address.sin_port)) <= 0)
    {
        goto error;
    }
    server->crl_port = ntohs(crl_address.sin_port);
    server_key = generate_key();
    if (server_key == NULL ||
        (leaf = generate_certificate(server_key, ca->certificate, ca->key, san, crl_url, NULL, ca->next_serial++)) == NULL ||
        !has_valid_ca_chain(leaf, ca) ||
        !capture_leaf_ip_san(server, leaf))
    {
        goto error;
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    server->context = SSL_CTX_new(SSLv23_server_method());
#else
    server->context = SSL_CTX_new(TLS_server_method());
#endif
    if (server->context == NULL ||
        SSL_CTX_use_certificate(server->context, leaf) != 1 ||
        SSL_CTX_use_PrivateKey(server->context, server_key) != 1 ||
        SSL_CTX_check_private_key(server->context) != 1)
    {
        goto error;
    }
    server->listener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (server->listener == INVALID_TEST_SOCKET)
    {
        goto error;
    }
    (void)setsockopt(server->listener, IPPROTO_IPV6, IPV6_V6ONLY,
        (const char*)&ipv6_only, sizeof(ipv6_only));
    memset(&address, 0, sizeof(address));
    address.sin6_family = AF_INET6;
    address.sin6_addr.s6_addr[15] = 1;
    if (bind(server->listener, (const struct sockaddr*)&address, sizeof(address)) != 0 ||
        getsockname(server->listener, (struct sockaddr*)&address, &address_size) != 0 ||
        listen(server->listener, 1) != 0)
    {
        goto error;
    }
    server->port = ntohs(address.sin6_port);
    if (ThreadAPI_Create(&server->crl_thread, crl_thread, server) != THREADAPI_OK)
    {
        goto error;
    }
    if (ThreadAPI_Create(&server->thread, server_thread, server) != THREADAPI_OK)
    {
        goto error;
    }
    X509_free(leaf);
    EVP_PKEY_free(server_key);
    return server;

error:
    X509_free(leaf);
    EVP_PKEY_free(server_key);
    (void)tls_test_server_stop(server, NULL);
    return NULL;
}

int tls_test_server_port(const TLS_TEST_SERVER* server)
{
    return server == NULL ? 0 : server->port;
}

size_t tls_test_server_leaf_ip_san(
    const TLS_TEST_SERVER* server, unsigned char* address, size_t address_size)
{
    size_t result = 0;
    if (server != NULL && address != NULL
        && server->leaf_ip_san_length <= address_size)
    {
        memcpy(address, server->leaf_ip_san, server->leaf_ip_san_length);
        result = server->leaf_ip_san_length;
    }
    return result;
}

int tls_test_server_stop(TLS_TEST_SERVER* server, int* crl_served)
{
    int accepted = 0;
    if (server == NULL)
    {
        return -1;
    }
    if (server->thread != NULL)
    {
        if (ThreadAPI_Join(server->thread, NULL) != THREADAPI_OK)
        {
            return -1;
        }
        accepted = server->accepted;
    }
    if (server->crl_thread != NULL)
    {
        struct sockaddr_in address;
        TEST_SOCKET wake = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (wake != INVALID_TEST_SOCKET)
        {
            memset(&address, 0, sizeof(address));
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons((unsigned short)server->crl_port);
            if (connect(wake, (const struct sockaddr*)&address, sizeof(address)) == 0)
            {
                (void)send(wake, "!", 1, 0);
            }
            (void)close_test_socket(wake);
        }
        if (ThreadAPI_Join(server->crl_thread, NULL) != THREADAPI_OK)
        {
            return -1;
        }
    }
    if (crl_served != NULL)
    {
        *crl_served = server->crl_served;
    }
    if (server->listener != INVALID_TEST_SOCKET)
    {
        (void)close_test_socket(server->listener);
    }
    if (server->crl_listener != INVALID_TEST_SOCKET)
    {
        (void)close_test_socket(server->crl_listener);
    }
    SSL_CTX_free(server->context);
    free(server->crl_der);
#ifdef _WIN32
    if (server->winsock_started)
    {
        (void)WSACleanup();
    }
#endif
    free(server);
    return accepted;
}
