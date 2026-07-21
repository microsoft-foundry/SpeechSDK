//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "include/web_socket_redirect_delegate.h"
#include "create_object_helpers.h"
#include "site_helpers.h"
#include "web_socket_message.h"
#include "web_socket_url_cache.h"
#include "interfaces/ispx_http_transport_factory.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

void CSpxRedirectingWebSocket::Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    m_lastConnectionEndpointInfo = webSocketEndpoint.Clone();
    m_connectionId = connectionId;

    // Check cache for redirect URL
    std::string cachedUrl;
    if (m_useWebSocketUrlCache && EndpointCache::getInstance().get(webSocketEndpoint.EndpointUrl(), cachedUrl))
    {
        // Use cached redirect URL
        m_lastConnectionEndpointInfo->EndpointUrl(cachedUrl);
        WS_Child::Connect(*m_lastConnectionEndpointInfo, connectionId);
    }
    else
    {
        // No cache hit, use original URL
        WS_Child::Connect(webSocketEndpoint, connectionId);
    }
}

void CSpxRedirectingWebSocket::InitDelegatePtr(ISpxWebSocket::Ptr& targetWebSocket)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    auto transportFactory = SpxQueryService<ISpxHttpTransportFactory>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, transportFactory == nullptr);

    auto nearestProperties = SpxQueryService<ISpxNamedProperties>(GetSite());
    // There may not be a property collection, in which case the transport factory will use the root
    // site's properties.

    targetWebSocket = transportFactory->CreateWebSocket(nearestProperties, GetSite());
    if (targetWebSocket != nullptr)
    {
        ISpxWebSocketDelegateImpl::InitDelegatePtr(targetWebSocket);
        return;
    }

    ZombieCSpxWebSocketDelegate(true);
    SPX_DBG_TRACE_WARNING("Couldn't create engine adapter; zombified...");

    ExceptionWithCallStack ex(SPXERR_COULD_NOT_CREATE_ENGINE_ADAPTER);

    throw ex;
}

void CSpxRedirectingWebSocket::InitDelegatePtr(ISpxWebSocketInit::Ptr& ptr)
{
    ptr = SpxQueryInterface<ISpxWebSocketInit>(CSpxWebSocketDelegateHelper::GetDelegate());
}

void CSpxRedirectingWebSocket::OnErrorCallback(const ISpxErrorInformation::Ptr& errorInfo)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    if (errorInfo->GetCancellationCode() == CancellationErrorCode::ServiceRedirectPermanent ||
        errorInfo->GetCancellationCode() == CancellationErrorCode::ServiceRedirectTemporary)
    {
        auto headers = errorInfo->GetDetails();

        auto headers_lower = PAL::StringUtils::ToLower(headers);

        // Parse max-cache-age if present
        size_t maxAgeStart = headers_lower.find("cache-control: max-age=");
        int cacheAge = 0;
        if (maxAgeStart != std::string::npos)
        {
            maxAgeStart += 23; // Length of "cache-control: max-age="
            size_t maxAgeEnd = headers_lower.find("\r\n", maxAgeStart);
            std::string maxAgeStr = headers_lower.substr(maxAgeStart, maxAgeEnd - maxAgeStart);
            try
            {
                cacheAge = std::stoi(maxAgeStr);
            } catch (...)
            {
                SPX_TRACE_WARNING("Invalid max-cache-age value: %s", maxAgeStr.c_str());
            }
        }

        // Get redirect URL from headers
        size_t locationStart = headers_lower.find("location: ");
        std::string redirectUrl;

        if (locationStart != std::string::npos)
        {
            locationStart += 10; // Length of "location: "
            size_t locationEnd = headers_lower.find("\r\n", locationStart);
            // Read the URL from original headers to preserve character case
            redirectUrl = headers.substr(locationStart, locationEnd - locationStart);
        }
        
        SPX_TRACE_INFO("Redirection to %s", redirectUrl.c_str());

        // Cache based on redirect type
        if (cacheAge > 0)
        {
            // Cache temporary redirects only if max-cache-age is present and positive
            EndpointCache::getInstance().put(m_lastConnectionEndpointInfo->EndpointUrl(),
                                          redirectUrl,
                                          std::chrono::seconds(cacheAge));
        }
        else if (errorInfo->GetCancellationCode() == CancellationErrorCode::ServiceRedirectPermanent)
        {
            // Cache permanent redirects for 1 week
            EndpointCache::getInstance().put(m_lastConnectionEndpointInfo->EndpointUrl(),
                redirectUrl,
                m_webSocketUrlCacheValidity); // 1 week
        }

        m_lastConnectionEndpointInfo->EndpointUrl(redirectUrl);

        WS_Child::ZombieTermAndClear();
        WS_Child::Zombie(false);

        Init_Child::ZombieTermAndClear();
        Init_Child::Zombie(false);

        Init_Child::Init(m_threadService, m_affinity, m_pollingIntervalMs, m_telemetry, m_httpErrorHandler);
        WS_Child:: Connect(*m_lastConnectionEndpointInfo, m_connectionId);

    }
    else
    {
        ISpxWebSocketDelegateImpl::OnErrorCallback(errorInfo);
    }
}

void CSpxRedirectingWebSocket::Init(const std::shared_ptr<ISpxThreadService>& threadService,
    const ISpxThreadService::Affinity affinity,
    const std::chrono::milliseconds& pollingIntervalMs,
    const std::shared_ptr<ISpxWebSocketTelemetry>& telemetry,
    const std::shared_ptr<ISpxHttpErrorHandler>& httpErrorHandler)
{
    m_threadService = threadService;
    m_affinity = affinity;
    m_pollingIntervalMs = pollingIntervalMs;
    m_telemetry = telemetry;
    m_httpErrorHandler = httpErrorHandler;

    auto site = GetSite();
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, site == nullptr);

    auto props = SpxQueryService<ISpxNamedProperties>(site);
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, props == nullptr);

    m_useWebSocketUrlCache = props->GetOr<bool>("SPEECH-WEBSOCKET_USE_REDIRECT_CACHE", true);
    m_webSocketUrlCacheValidity = std::chrono::seconds(props->GetOr<int>("SPEECH-WEBSOCKET_URL_CACHE_VALIDITY_SECONDS", 7 * 24 * 60 * 60));

    Init_Child::Init(threadService, affinity, pollingIntervalMs, telemetry, httpErrorHandler);
}

void CSpxRedirectingWebSocket::SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs)
{
    // Update our local copy of the polling interval for use when reconnecting after redirects
    m_pollingIntervalMs = pollingIntervalMs;

    // Delegate to the underlying web socket
    Init_Child::SetPollingInterval(pollingIntervalMs);
}

}}}}
