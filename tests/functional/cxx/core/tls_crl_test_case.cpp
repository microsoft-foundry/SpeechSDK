//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// TLS/CRL validation tests against the tls-test-container Docker container.
//
// Prerequisite: docker run -d -p 8080:8080 -p 9001-9021:9001-9021 tls-test-container
//
// If the test runner cannot reach localhost (e.g., running inside a different
// container), set TLS_TEST_HOST to the IP/hostname of the Docker host.
// Note: certificates have SANs for localhost/127.0.0.1 only, so the host
// value must resolve to the same machine where ports are mapped.
//

#include "stdafx.h"

#include <chrono>
#include <cstdlib>
#include <future>
#include <mutex>
#include <string>
#include <thread>

#include "thread_service.h"
#include "test_utils.h"
#include "site_helpers.h"
#include "usp.h"
#include "guid_utils.h"
#include "create_object_helpers.h"
#include "interfaces/ispx_usp_connection.h"
#include "interfaces/ispx_http_request.h"
#include "interfaces/ispx_http_response.h"
#include "interfaces/ispx_http_transport_factory.h"
#include "interfaces/http_method.h"
#include "http_endpoint_info.h"

using namespace std;
using namespace std::chrono_literals;
using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// The container serves the root CA cert at http://<host>:8080/root-ca.pem.
// We fetch it once at first use and cache it for the process lifetime.
//
// Set TLS_TEST_HOST to override the default (localhost), e.g. when the
// test container runs on a different host or docker bridge IP.

// Port assignments matching the container's manifest
static const int PORT_VALID_CERT = 9001;
static const int PORT_REVOKED_CERT = 9002;
static const int PORT_EXPIRED_CRL = 9003;
static const int PORT_NO_NEXTUPDATE_CRL = 9004;
static const int PORT_INDIRECT_CRL = 9005;
static const int PORT_SELF_ISSUED_INTERMEDIATE = 9006;
static const int PORT_HTTPS_CDP_ONLY = 9007;
static const int PORT_DELTA_CRL = 9008;
static const int PORT_NO_CDP = 9009;
static const int PORT_EXPIRED_CERT = 9010;
static const int PORT_WRONG_HOSTNAME = 9011;
static const int PORT_SELF_SIGNED_LEAF = 9012;
static const int PORT_UNTRUSTED_ROOT = 9013;
static const int PORT_PARTITIONED_VALID = 9014;
static const int PORT_PARTITIONED_REVOKED_COMPROMISE = 9015;
static const int PORT_PARTITIONED_REVOKED_ROUTINE = 9016;
static const int PORT_USER_SCOPE_CRL = 9017;
// static const int PORT_CA_SCOPE_CRL = 9018;
static const int PORT_WRONG_SCOPE_CRL = 9019;
static const int PORT_SHARDED_CRL_ENDPOINT1 = 9020;
static const int PORT_SHARDED_CRL_ENDPOINT2 = 9021;

// ---------------------------------------------------------------------------
// Helper: resolve the TLS test container host
// ---------------------------------------------------------------------------

static const std::string& GetTestHost()
{
    static std::string s_host;
    static std::once_flag s_flag;
    std::call_once(s_flag, []()
    {
        auto env = PAL::SpxGetEnv("TLS_TEST_HOST");
        s_host = env.GetOr("localhost");
    });
    return s_host;
}

// ---------------------------------------------------------------------------
// Helper: HTTP GET using the platform HTTP stack
// ---------------------------------------------------------------------------

static std::string HttpGet(const std::string& url)
{
    try
    {
        HttpEndpointInfo endpoint(url);
        auto site = SpxGetRootSite();
        auto networkFactory = SpxQueryService<ISpxHttpTransportFactory>(site);
        auto request = networkFactory->CreateHttpRequest();
        auto response = request->SendRequest(HttpMethod::Get, endpoint);
        if (!response->IsSuccess())
        {
            return {};
        }
        return response->ReadContentAsString();
    }
    catch (...)
    {
        return {};
    }
}

// ---------------------------------------------------------------------------
// Helper: fetch and cache root CA from the test container
// ---------------------------------------------------------------------------

static const std::string& GetTestRootCa()
{
    static std::string s_rootCa;
    static std::once_flag s_flag;
    std::call_once(s_flag, []()
    {
        s_rootCa = HttpGet("http://" + GetTestHost() + ":8080/root-ca.pem");
    });
    return s_rootCa;
}

// ---------------------------------------------------------------------------
// Helper: check if container is reachable
// ---------------------------------------------------------------------------

static bool IsContainerRunning()
{
    return !HttpGet("http://" + GetTestHost() + ":8080/health").empty();
}

// ---------------------------------------------------------------------------
// TlsTestClient: connects to a WSS endpoint with custom trust config
// ---------------------------------------------------------------------------

enum class ConnectResult
{
    Unknown,
    Connected,
    Failed
};

class TlsTestClient : public USP::Callbacks, public std::enable_shared_from_this<TlsTestClient>
{
public:
    TlsTestClient(int port,
                   const std::string& trustedCert,
                   bool disableCrl = false,
                   bool continueOnCrlFailure = false)
        : m_port(port)
        , m_trustedCert(trustedCert)
        , m_disableCrl(disableCrl)
        , m_continueOnCrlFailure(continueOnCrlFailure)
        , m_connectResult(ConnectResult::Unknown)
    {
    }

    ConnectResult TryConnect(std::chrono::seconds timeout = 15s)
    {
        std::string url = "wss://" + GetTestHost() + ":" + std::to_string(m_port) + "/";

        std::array<std::string, static_cast<size_t>(USP::AuthenticationType::SIZE_AUTHENTICATION_TYPE)> authData;
        authData[static_cast<size_t>(USP::AuthenticationType::SubscriptionKey)] = "test-key";

        auto clientConfig = USP::ClientConfiguration(shared_from_this(), USP::EndpointType::Speech, PAL::CreateGuidWithoutDashesUTF8())
            .SetRegion("westus")
            .SetEndpointUrl(url)
            .SetAuthentication(authData);

        if (!m_trustedCert.empty())
        {
            clientConfig.SetSingleTrustedCert(m_trustedCert);
        }

        clientConfig.SetDisableCrlChecks(m_disableCrl);
        clientConfig.SetContinueOnCrlDownloadFailure(m_continueOnCrlFailure);

        m_threadService = SpxCreateObjectWithSite<ISpxThreadService>("CSpxSiteWithThreadService", SpxGetRootSite());
        auto tsSite = SpxQueryInterface<ISpxGenericSite>(m_threadService);

        m_connection = SpxCreateObjectWithSite<ISpxUspConnection>("CSpxUspConnection", tsSite);
        m_connection->SetConfiguration(clientConfig);
        m_connection->Connect();

        // Send a small data chunk to trigger the async connection
        constexpr unsigned int dataSize = 7;
        auto data = new uint8_t[dataSize]{ 1, 2, 3, 4, 5, 6, 7 };
        std::shared_ptr<uint8_t> buffer(data, [](uint8_t* p) { delete[] p; });
        m_connection->QueueAudioSegment(std::make_shared<DataChunk>(buffer, dataSize));

        // Wait for either OnConnected or OnError
        auto future = m_promise.get_future();
        auto status = future.wait_for(timeout);

        if (status == std::future_status::timeout)
        {
            m_connectResult = ConnectResult::Failed;
            m_errorDetails = "Timeout waiting for connection result";
        }

        // Allow a brief window for any additional callbacks
        std::this_thread::sleep_for(1s);

        return m_connectResult;
    }

    void Cleanup()
    {
        if (m_threadService)
        {
            auto term = SpxQueryInterface<ISpxObjectInit>(m_threadService);
            term->Term();
        }
    }

    ConnectResult GetResult() const { return m_connectResult; }
    const std::string& GetErrorDetails() const { return m_errorDetails; }

    virtual ~TlsTestClient() = default;

protected:
    void OnConnected(const std::string& url) override
    {
        (void)url;
        m_connectResult = ConnectResult::Connected;
        if (!m_promiseSet.exchange(true))
        {
            m_promise.set_value();
        }
    }

    void OnError(const std::shared_ptr<ISpxErrorInformation>& error) override
    {
        m_connectResult = ConnectResult::Failed;
        m_errorDetails = error->GetDetails();
        if (!m_promiseSet.exchange(true))
        {
            m_promise.set_value();
        }
    }

private:
    int m_port;
    std::string m_trustedCert;
    bool m_disableCrl;
    bool m_continueOnCrlFailure;

    ISpxUspConnection::Ptr m_connection;
    ISpxThreadService::Ptr m_threadService;

    std::promise<void> m_promise;
    std::atomic<bool> m_promiseSet{false};
    ConnectResult m_connectResult;
    std::string m_errorDetails;
};

// ---------------------------------------------------------------------------
// Helper: create a client and attempt connection
// ---------------------------------------------------------------------------

static ConnectResult TryTlsConnect(int port, const std::string& rootCaPem,
                                   bool disableCrl = false,
                                   bool continueOnCrlFailure = false,
                                   std::string* errorDetails = nullptr)
{
    auto client = std::make_shared<TlsTestClient>(port, rootCaPem, disableCrl, continueOnCrlFailure);
    auto result = client->TryConnect();
    if (errorDetails)
    {
        *errorDetails = client->GetErrorDetails();
    }
    client->Cleanup();
    return result;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

SPXTEST_CASE_BEGIN("TLS/CRL: Container prerequisite check", "[tls][crl]")
{
    if (!IsContainerRunning())
    {
        WARN("TLS test container not running at " << GetTestHost() << ":8080. "
             "Start it with: docker run -d -p 8080:8080 -p 9001-9021:9001-9021 tls-test-container. "
             "Set TLS_TEST_HOST if running from a different container.");
        SPXTEST_REQUIRE(false);
    }

    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());
    INFO("Fetched root CA from container (" << rootCa.size() << " bytes)");
}SPXTEST_CASE_END()

// ---------------------------------------------------------------------------
// Basic TLS scenarios
// ---------------------------------------------------------------------------

SPXTEST_CASE_BEGIN("TLS: Valid certificate connects successfully", "[tls]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    // Disable CRL for this basic TLS-only test
    auto result = TryTlsConnect(PORT_VALID_CERT, rootCa, /*disableCrl=*/true);
    SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("TLS: Expired certificate fails", "[tls]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_EXPIRED_CERT, rootCa, /*disableCrl=*/true, false, &errorDetails);
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("TLS: Wrong hostname fails", "[tls]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_WRONG_HOSTNAME, rootCa, /*disableCrl=*/true, false, &errorDetails);
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("TLS: Self-signed leaf fails", "[tls]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_SELF_SIGNED_LEAF, rootCa, /*disableCrl=*/true, false, &errorDetails);
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("TLS: Untrusted root fails", "[tls]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_UNTRUSTED_ROOT, rootCa, /*disableCrl=*/true, false, &errorDetails);
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

// ---------------------------------------------------------------------------
// CRL scenarios
// ---------------------------------------------------------------------------

SPXTEST_CASE_BEGIN("CRL: Valid cert with valid CRL connects", "[tls][crl]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    auto result = TryTlsConnect(PORT_VALID_CERT, rootCa);
    SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: Revoked certificate fails", "[tls][crl]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_REVOKED_CERT, rootCa, false, false, &errorDetails);
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: Expired CRL triggers re-download or failure", "[tls][crl]")
{
    // Finding #3: expired CRL (nextUpdate in the past)
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_EXPIRED_CRL, rootCa, false, false, &errorDetails);
    // With an expired CRL, the code should evict the cached CRL and try to
    // re-download. The server serves the expired CRL again, so the check
    // should ultimately fail.
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: Missing nextUpdate causes repeated downloads", "[tls][crl]")
{
    // Finding #3: CRL with nextUpdate field omitted.
    // crl_valid() returns 0 for absent nextUpdate, so the CRL is never cached.
    // With continue_on_crl_download_failure=true, connection should succeed
    // but the CRL is re-downloaded every time.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_NO_NEXTUPDATE_CRL, rootCa, false, /*continueOnCrlFailure=*/true, &errorDetails);
    // Note: The actual behavior depends on whether the CRL download succeeds.
    // If it does, OpenSSL will accept the CRL (it treats absent nextUpdate
    // as acceptable). If crl_valid() prevents caching and the re-download
    // fails, connection fails.
    // We test with continueOnCrlFailure=true to isolate the caching issue.
    (void)result; // Document observed behavior rather than assert
    INFO("no-nextupdate-crl result: " << (result == ConnectResult::Connected ? "connected" : "failed"));
    INFO("error: " << errorDetails);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: Indirect CRL is rejected (finding #4)", "[tls][crl]")
{
    // Finding #4: is_valid_crl() compares CRL issuer against cert issuer,
    // but for indirect CRLs the CRL issuer differs. The code rejects
    // the CRL before OpenSSL can evaluate it.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_INDIRECT_CRL, rootCa, false, false, &errorDetails);
    // KNOWN BUG: This should succeed (the indirect CRL is valid and the cert
    // is not revoked), but is_valid_crl() rejects it because the CRL issuer
    // (CA A) doesn't match the certificate issuer (CA B).
    INFO("indirect-crl result: " << (result == ConnectResult::Connected ? "connected" : "failed"));
    INFO("error: " << errorDetails);
    // When the bug is fixed, change this to:
    // SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: Self-issued intermediate skips CRL check (finding #6)", "[tls][crl]")
{
    // Finding #6: crls_http_cb() returns an empty CRL stack for certs where
    // issuer == subject. check_cert_error_cb() then suppresses the error.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_SELF_ISSUED_INTERMEDIATE, rootCa, false, false, &errorDetails);
    // The self-issued intermediate's CRL check is skipped entirely.
    // Connection will succeed regardless of revocation status.
    INFO("self-issued-intermediate result: " << (result == ConnectResult::Connected ? "connected" : "failed"));
    INFO("error: " << errorDetails);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: HTTPS-only CDP fails to fetch CRL (finding #9)", "[tls][crl]")
{
    // Finding #9: get_dp_url() only matches http:// URIs, and
    // load_cert_crl_http() explicitly rejects https://.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_HTTPS_CDP_ONLY, rootCa, false, false, &errorDetails);
    // CRL download will fail because the CDP URL uses https://
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: Delta CRL support", "[tls][crl]")
{
    // Finding #7: delta CRL handling. The cert has a freshestCRL extension
    // pointing to the delta CRL. OpenSSL validates the delta against the base.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    auto result = TryTlsConnect(PORT_DELTA_CRL, rootCa);
    // Should succeed — valid base CRL + valid delta CRL, cert not revoked
    SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: No CDP with continue-on-failure succeeds", "[tls][crl]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    auto result = TryTlsConnect(PORT_NO_CDP, rootCa, false, /*continueOnCrlFailure=*/true);
    SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL: No CDP without continue-on-failure", "[tls][crl]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_NO_CDP, rootCa, false, false, &errorDetails);
    // With no CDP, the CRL callback returns an empty stack. The verify
    // callback (check_cert_error_cb) suppresses UNABLE_TO_GET_CRL for
    // self-signed certs in the chain, which may allow the connection
    // to succeed even without continue_on_failure being set.
    INFO("no-cdp result: " << (result == ConnectResult::Connected ? "connected" : "failed"));
    INFO("error: " << errorDetails);
}SPXTEST_CASE_END()

// ---------------------------------------------------------------------------
// CRL Partitioning scenarios (finding #11)
// ---------------------------------------------------------------------------

SPXTEST_CASE_BEGIN("CRL Partitioning: Valid cert with two partitions connects", "[tls][crl][partition]")
{
    // Cert has 2 CDPs (compromise + routine partitions), not revoked on either
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    auto result = TryTlsConnect(PORT_PARTITIONED_VALID, rootCa);
    SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL Partitioning: Revoked on first partition (compromise) fails", "[tls][crl][partition]")
{
    // Cert revoked for keyCompromise — listed on partition 1 CRL.
    // Since load_crl_crldp() returns the first CRL it finds, and
    // the compromise partition is listed first in the CDP, this should fail.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_PARTITIONED_REVOKED_COMPROMISE, rootCa, false, false, &errorDetails);
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL Partitioning: Revoked on second partition (routine) - finding #11", "[tls][crl][partition]")
{
    // CRITICAL TEST for finding #11.
    // Cert is revoked for cessationOfOperation, which appears ONLY on
    // partition 2's CRL (routine reasons). Partition 1 (compromise reasons)
    // does not list this cert.
    //
    // BUG: load_crl_crldp() returns the first CRL it finds (partition 1)
    // and short-circuits. The revocation on partition 2 is never seen.
    // The connection succeeds when it should fail.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_PARTITIONED_REVOKED_ROUTINE, rootCa, false, false, &errorDetails);

    // EXPECTED CORRECT BEHAVIOR: connection should fail (cert is revoked)
    // ACTUAL BEHAVIOR (bug #11): connection succeeds because only first
    // partition is checked.
    INFO("partitioned-revoked-routine result: " << (result == ConnectResult::Connected ? "connected" : "failed"));
    INFO("error: " << errorDetails);
    // When bug #11 is fixed, uncomment:
    // SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL Partitioning: User-scope CRL for end-entity cert", "[tls][crl][partition]")
{
    // CRL has IDP onlyContainsUserCerts=TRUE, cert is an end-entity cert.
    // OpenSSL should accept this CRL for the cert.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    auto result = TryTlsConnect(PORT_USER_SCOPE_CRL, rootCa);
    SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL Partitioning: Wrong scope CRL rejected by OpenSSL", "[tls][crl][partition]")
{
    // End-entity cert but CRL has onlyContainsCACerts=TRUE.
    // OpenSSL should reject this CRL (scope mismatch).
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    std::string errorDetails;
    auto result = TryTlsConnect(PORT_WRONG_SCOPE_CRL, rootCa, false, false, &errorDetails);
    SPXTEST_REQUIRE(result == ConnectResult::Failed);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("CRL Sharding: Sequential connections to different CRL partitions", "[tls][crl][partition]")
{
    // Reproduces real-world sharded CRL bug (e.g., Microsoft's Partition00081.crl
    // vs Partition00082.crl). Two leaf certs from the SAME CA each have a CDP
    // pointing to a DIFFERENT CRL shard file. Each CRL has an IDP with
    // onlyContainsUserCerts=TRUE and its own URL.
    //
    // Connect to endpoint 1 first — this downloads and caches shard A's CRL.
    // Then connect to endpoint 2 — this needs shard B's CRL.
    //
    // BUG (old code): CRL was cached by issuer name, so the second connection
    // would get shard A's CRL. OpenSSL would see the IDP URL mismatch and
    // reject it, failing the handshake.
    //
    // FIX: CRL is cached by distribution point URL, so each shard is cached
    // independently.
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    // First connection: primes the CRL cache with shard A
    std::string errorDetails1;
    auto result1 = TryTlsConnect(PORT_SHARDED_CRL_ENDPOINT1, rootCa, false, true, &errorDetails1);
    INFO("sharded endpoint 1 result: " << (result1 == ConnectResult::Connected ? "connected" : "failed"));
    INFO("endpoint 1 error: " << errorDetails1);
    SPXTEST_REQUIRE(result1 == ConnectResult::Connected);

    // Second connection: must download shard B, not reuse shard A from cache
    std::string errorDetails2;
    auto result2 = TryTlsConnect(PORT_SHARDED_CRL_ENDPOINT2, rootCa, false, true, &errorDetails2);
    INFO("sharded endpoint 2 result: " << (result2 == ConnectResult::Connected ? "connected" : "failed"));
    INFO("endpoint 2 error: " << errorDetails2);
    SPXTEST_REQUIRE(result2 == ConnectResult::Connected);
}SPXTEST_CASE_END()

// ---------------------------------------------------------------------------
// CRL disabled scenarios
// ---------------------------------------------------------------------------

SPXTEST_CASE_BEGIN("CRL: Disabled CRL check allows revoked cert", "[tls][crl]")
{
    const auto& rootCa = GetTestRootCa();
    SPXTEST_REQUIRE(!rootCa.empty());

    // With CRL checks disabled, even a revoked cert should connect
    auto result = TryTlsConnect(PORT_REVOKED_CERT, rootCa, /*disableCrl=*/true);
    SPXTEST_REQUIRE(result == ConnectResult::Connected);
}SPXTEST_CASE_END()
