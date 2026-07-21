//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "mocks.h"
#include "test_utils.h"
#include "mock_usp_service.h"

#include "error_info.h"
#include "interfaces/named_properties.h"
#include "interfaces/i_http_endpoint_info.h"
#include "create_object_helpers.h"
#include "http_endpoint_info.h"
#include "guid.h"
#include "web_socket_message.h"
#include "site_helpers.h"
#include "interfaces/ispx_http_transport_factory.h"

struct CallReport
{
public:
    void Assert(bool test, std::string message)
    {
        m_assertions.emplace_back(Assertion{ test, std::move(message) });
    }

    bool IsSuccess() const
    {
        return IsSuccess(1);
    }

    bool IsSuccess(size_t expected) const
    {
        if (!WasCalled(expected))
        {
            AZAC_TRACE_ERROR("Was not called");
            return false;
        }
        bool success{ true };
        for (auto& assertion : m_assertions)
        {
            if (!assertion.Status)
            {
                success = false;
                AZAC_TRACE_ERROR("Assertion failed: %s", assertion.Message.c_str());
            }
        }
        return success;

    }

    operator bool() const
    {
        return IsSuccess();
    }

    bool WasCalled() const
    {
        return WasCalled(1);
    }

    bool WasCalled(size_t expected) const
    {
        return m_calledCount.load() == expected;
    }

    template<typename R, typename P>
    bool WaitFor(size_t expected, const std::chrono::duration<R, P>& duration)&
    {
        return m_callAwaiter.WaitFor(duration, [&]()
            {
                return WasCalled(expected);
            });
    }

    template<typename R, typename P>
    bool WaitFor(const std::chrono::duration<R, P>& duration)&
    {
        return m_callAwaiter.WaitFor(duration, [&]()
            {
                return WasCalled(1);
            });
    }

    void Signal()
    {
        m_calledCount.fetch_add(1);
        m_callAwaiter.Signal();
    }

private:
    struct Assertion
    {
        bool Status;
        std::string Message;
    };

    Awaiter m_callAwaiter{};
    std::atomic<size_t> m_calledCount{ 0 };
    std::vector<Assertion> m_assertions{};

};

SPXTEST_CASE_BEGIN("Websocket -- Queuing", "[network][usp]")
{
    constexpr auto requiredPollingInterval{ 100ms };

    auto proxyFactory = std::make_shared<ObjectFactoryProxy>();

    CallReport webSocketInitCallReport{};
    CallReport websocketConnectCallReport{};
    CallReport websocketDataCallReport{};
    CallReport websocketSendMessageCallReport{};

    auto mockSite = std::make_shared<MockSite>();
    mockSite->AddService<Carbon::ISpxObjectFactory>(proxyFactory);

    auto queuingSocket = Carbon::SpxCreateObjectWithSite<Carbon::ISpxWebSocket>("CSpxQueuingWebSocket", mockSite);
    auto mockWebSocket = new WebSocket{};
    auto state = Carbon::WebSocketState::INITIAL;

    proxyFactory->RegisterOverride<Carbon::ISpxWebSocket>("CSpxRedirectingWebSocket", [&]()
        {        
            mockWebSocket->InitHandler = [&](const Carbon::ISpxThreadService::Ptr& threadService,
                const Carbon::ISpxThreadService::Affinity,
                const std::chrono::milliseconds& pollingInterval,
                const Carbon::ISpxWebSocketTelemetry::Ptr&,
                const Carbon::ISpxHttpErrorHandler::Ptr&)
                {
                    webSocketInitCallReport.Assert(threadService != nullptr, "Received a valid thread service instance");
                    webSocketInitCallReport.Assert(pollingInterval == requiredPollingInterval, "Polling interval matches expected value");
                    webSocketInitCallReport.Signal();
                };

            mockWebSocket->ObjInitHandler = [&, mockWebSocket]()
                {
                    auto ws = static_cast<Carbon::ISpxWebSocket*>(mockWebSocket)->shared_from_this();
                    state = Carbon::WebSocketState::INITIAL;
                };

            mockWebSocket->ConnectHandler = [&](const Carbon::IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId)
                {
                    websocketConnectCallReport.Assert(webSocketEndpoint.EndpointUrl().find("tacos") != std::string::npos, "Got a valid endpoint info");
                    websocketConnectCallReport.Assert(!connectionId.empty(), "Valid Connection ID");
                    state = Carbon::WebSocketState::CONNECTED;
                    websocketConnectCallReport.Signal();
                };

            mockWebSocket->SendDataHandler = [&](const Carbon::IWebSocketMessage::Ptr&)
                {
                    websocketDataCallReport.Signal();
                };

            mockWebSocket->SendBinaryDataHandler = [&](const uint8_t*, const size_t)
                {
                    websocketSendMessageCallReport.Signal();
                };

            mockWebSocket->SendTextDataHandler = [&](const std::string&)
                {
                    websocketSendMessageCallReport.Signal();
                };

            mockWebSocket->GetStateHandler = [&]() -> Carbon::WebSocketState
                {
                    return state;
                };
            return static_cast<Carbon::ISpxWebSocket*>(mockWebSocket);
        });

    Carbon::HttpEndpointInfo endpointInfo("wss://www.tacos.com");

    uint8_t data[9];

    queuingSocket->SendBinaryData(data, 0);
    queuingSocket->SendTextData("Some text");

    auto message = std::make_shared<Carbon::WebSocketMessage>("MessageText");

    queuingSocket->SendData(message);

    SPXTEST_REQUIRE(websocketDataCallReport.WasCalled(0));
    SPXTEST_REQUIRE(websocketSendMessageCallReport.WasCalled(0));

    queuingSocket->Connect(endpointInfo, PAL::GenerateGUID());
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(1));
    SPXTEST_REQUIRE(websocketSendMessageCallReport.WasCalled(0));
    SPXTEST_REQUIRE(websocketDataCallReport.WasCalled(0));

    mockWebSocket->OnConnected("https://www.connected.com");
    SPXTEST_REQUIRE(websocketSendMessageCallReport.WasCalled(0));
    SPXTEST_REQUIRE(websocketDataCallReport.WasCalled(3));

    queuingSocket->SendBinaryData(data, 0);
    queuingSocket->SendTextData("Some text");
    queuingSocket->SendData(message);
    SPXTEST_REQUIRE(websocketSendMessageCallReport.WasCalled(2));
    SPXTEST_REQUIRE(websocketDataCallReport.WasCalled(4));

}

std::string GenerateErrorText(std::string nextUrl, int cacheDuration, int HTTP_STATUS)
{
    std::ostringstream errorText;

    errorText << "HTTP/1.1 " << HTTP_STATUS << " Moved\r\n";

    if (0 < cacheDuration)
    {
        errorText << "Cache-Control: max-age=" << cacheDuration << "\r\n";
    }

    errorText << "Content-Length: " << nextUrl.length() << "\r\n";
    errorText << "Location: " << nextUrl << "\r\n";
    errorText << "Date: Thu, 20 Feb 2025 15:01:19 GMT\r\n";
    errorText << "\r\n";
    errorText << nextUrl << "\r\n";
    return errorText.str();
}

SPXTEST_CASE_BEGIN("Websocket -- Redirect", "[network][usp]")
{
    constexpr auto requiredPollingInterval{ 100ms };

    auto proxyFactory = std::make_shared<ObjectFactoryProxy>();

    CallReport websocketConnectCallReport{};

    auto rootSite = Carbon::SpxGetRootSite();
    auto rootSiteProperties = rootSite->QueryInterface<Carbon::ISpxNamedProperties>();

    auto mockSite = std::make_shared<MockSite>();
    mockSite->AddService<Carbon::ISpxObjectFactory>(proxyFactory);
    mockSite->AddService<Carbon::ISpxHttpTransportFactory>(Carbon::SpxGetRootSite());
    mockSite->AddService<Carbon::ISpxNamedProperties>(rootSiteProperties);

    auto testSocket = Carbon::SpxCreateObjectWithSite<Carbon::ISpxWebSocket>("CSpxRedirectingWebSocket", mockSite);
    WebSocket* mockWebSocket;

    std::string anticipatedEndpoint = "";

    proxyFactory->RegisterOverride<Carbon::ISpxWebSocket>("CSpxWebSocket", [&]()
        {
            mockWebSocket = new WebSocket{};
                
            mockWebSocket->ConnectHandler = [&](const Carbon::IHttpEndpointInfo& webSocketEndpoint, const std::string&)
                {
                    // IF they contain each other, then they are the same.
                    SPXTEST_REQUIRE_STRING_CONTAINS(webSocketEndpoint.EndpointUrl().c_str(), anticipatedEndpoint.c_str(), Catch::CaseSensitive::Yes);
                    SPXTEST_REQUIRE_STRING_CONTAINS(anticipatedEndpoint.c_str(), webSocketEndpoint.EndpointUrl().c_str(), Catch::CaseSensitive::Yes);

                    websocketConnectCallReport.Signal();
                };
            return static_cast<Carbon::ISpxWebSocket*>(mockWebSocket);
        });

    auto initSocket = Carbon::SpxQueryInterface<Carbon::ISpxWebSocketInit>(testSocket);
    initSocket->Init(nullptr, Carbon::ISpxThreadService::Affinity::Background, requiredPollingInterval, nullptr, nullptr);

    auto resourcePath = "/some/path";
    auto queryParams = "?someParam=SomeValue";
    auto firstEndpoint = std::string("wss://www.firstcall.com") + resourcePath + queryParams;
    auto secondEndpoint = std::string("wss://www.secondcall.com") + resourcePath + queryParams;

    auto permRedirectEndpoint = std::string("https://www.permredirect.com") + resourcePath + queryParams;
    auto permRedirectEndpoint2 = std::string("https://www.permredirect2.com") + resourcePath + queryParams;
    auto tempRedirectEndpoint = std::string("https://www.tempredirect.com") + resourcePath + queryParams;

    anticipatedEndpoint = firstEndpoint;
    Carbon::HttpEndpointInfo endpointInfo(anticipatedEndpoint);

    SPX_TRACE_INFO("First call, just passes the connect.");
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(1));

    SPX_TRACE_INFO("Generate an error with a permanent redirect");
    anticipatedEndpoint = permRedirectEndpoint;
    auto errorText = GenerateErrorText(permRedirectEndpoint, 0, 301);
    auto error = Carbon::ErrorInfo::FromWebSocket(Carbon::WebSocketError::WEBSOCKET_UPGRADE, (int)Carbon::HttpStatusCode::MOVED_PERMANENTLY, errorText);
    mockWebSocket->OnError(error);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(2));

    SPX_TRACE_INFO("Try to call again w/ the original endpoint, and it should call the redirect.");
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(3));

    SPX_TRACE_INFO("Change endpoints.");
    anticipatedEndpoint = secondEndpoint;
    endpointInfo.EndpointUrl(secondEndpoint);
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(4));

    SPX_TRACE_INFO("Generate an error with a temporary redirect");
    errorText = GenerateErrorText(tempRedirectEndpoint, 0, 300);
    error = Carbon::ErrorInfo::FromWebSocket(Carbon::WebSocketError::WEBSOCKET_UPGRADE, (int)Carbon::HttpStatusCode::TEMP_REDIRECT, errorText);

    anticipatedEndpoint = tempRedirectEndpoint;
    mockWebSocket->OnError(error);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(5));

    SPX_TRACE_INFO("Call again with the second endpoint, and it should go through since the temp endpoint didn't have a cache time.");
    anticipatedEndpoint = secondEndpoint;
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(6));

    SPX_TRACE_INFO("Generate an error with a temporary redirect with a timeout.");
    errorText = GenerateErrorText(tempRedirectEndpoint, 2, 300);
    error = Carbon::ErrorInfo::FromWebSocket(Carbon::WebSocketError::WEBSOCKET_UPGRADE, (int)Carbon::HttpStatusCode::TEMP_REDIRECT, errorText);
    anticipatedEndpoint = tempRedirectEndpoint;
    mockWebSocket->OnError(error);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(7));

    SPX_TRACE_INFO("Quickly again....");
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(8));

    SPX_TRACE_INFO("Wait for the cache to expire.");
    std::this_thread::sleep_for(3s);
    anticipatedEndpoint = secondEndpoint;
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(9));

    SPX_TRACE_INFO("Now one last check for the perm redirect.");
    anticipatedEndpoint = permRedirectEndpoint;
    endpointInfo.EndpointUrl(firstEndpoint);
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(10));

    SPX_TRACE_INFO("Disable the cache and make sure it doesn't use it.");
    rootSiteProperties->Set<bool>("SPEECH-WEBSOCKET_USE_REDIRECT_CACHE", false);

    initSocket->Init(nullptr, Carbon::ISpxThreadService::Affinity::Background, requiredPollingInterval, nullptr, nullptr);

    anticipatedEndpoint = firstEndpoint;
    endpointInfo.EndpointUrl(firstEndpoint);
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(11));

    SPX_TRACE_INFO("Re-enable the cache, and make sure it uses it with a 5s perm redirect.");
    // Enable the cache, and set a permanent redirect duration of 5s.
    rootSiteProperties->Set<bool>("SPEECH-WEBSOCKET_USE_REDIRECT_CACHE", true);
    rootSiteProperties->Set<int>("SPEECH-WEBSOCKET_URL_CACHE_VALIDITY_SECONDS", 5);

    initSocket->Init(nullptr, Carbon::ISpxThreadService::Affinity::Background, requiredPollingInterval, nullptr, nullptr);

    errorText = GenerateErrorText(permRedirectEndpoint2, 5, 308);
    error = Carbon::ErrorInfo::FromWebSocket(Carbon::WebSocketError::WEBSOCKET_UPGRADE, (int)Carbon::HttpStatusCode::PERM_REDIRECT, errorText);
    anticipatedEndpoint = permRedirectEndpoint2;
    mockWebSocket->OnError(error);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(12));

    anticipatedEndpoint = permRedirectEndpoint2;
    endpointInfo.EndpointUrl(firstEndpoint);
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(13));

    std::this_thread::sleep_for(6s);
    anticipatedEndpoint = firstEndpoint;
    endpointInfo.EndpointUrl(firstEndpoint);
    testSocket->Connect(endpointInfo);
    SPXTEST_REQUIRE(websocketConnectCallReport.WasCalled(14));


}
