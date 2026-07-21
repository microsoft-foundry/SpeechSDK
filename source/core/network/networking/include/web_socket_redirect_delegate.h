//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <queue>
#include "web_socket_delegate_impl.h"
#include "web_socketInit_delegate_impl.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxRedirectingWebSocket :
    public ISpxWebSocketDelegateImpl<>,
    public ISpxWebSocketInitDelegateImpl<>,
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>
{
private:
    using WS_Child = ISpxWebSocketDelegateImpl<CSpxDelegateToSharedPtrHelper<ISpxWebSocket, true>>;
    using Init_Child = ISpxWebSocketInitDelegateImpl<CSpxDelegateToSharedPtrHelper<ISpxWebSocketInit>>;
public:
    CSpxRedirectingWebSocket() = default;
    ~CSpxRedirectingWebSocket() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        using namespace USP;
        SPX_INTERFACE_MAP_ENTRY(ISpxWebSocket)
        SPX_INTERFACE_MAP_ENTRY(ISpxWebSocketInit)
    SPX_INTERFACE_MAP_END()

    void Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId = "") override;

    void Init(const std::shared_ptr<ISpxThreadService>& threadService,
        const ISpxThreadService::Affinity affinity,
        const std::chrono::milliseconds& pollingIntervalMs,
        const std::shared_ptr<ISpxWebSocketTelemetry>& telemetry,
        const std::shared_ptr<ISpxHttpErrorHandler>& httpErrorHandler) override;

    void SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs) override;

        // Bring the base class Init() method into scope  
        using ISpxObjectWithSiteInitImpl<ISpxGenericSite>::Init;
protected:
    void InitDelegatePtr(ISpxWebSocket::Ptr& targetWebSocket) override;
    void InitDelegatePtr(ISpxWebSocketInit::Ptr& ptr) override;
    void OnErrorCallback(const ISpxErrorInformation::Ptr& errorInfo) override;
    
private:
    std::unique_ptr<IHttpEndpointInfo> m_lastConnectionEndpointInfo;
    std::string m_connectionId;
    ISpxThreadService::Ptr m_threadService;
    ISpxThreadService::Affinity m_affinity;
    std::chrono::milliseconds m_pollingIntervalMs;
    std::shared_ptr<ISpxWebSocketTelemetry> m_telemetry;
    ISpxHttpErrorHandler::Ptr m_httpErrorHandler;
    bool m_useWebSocketUrlCache = true;
    std::chrono::seconds m_webSocketUrlCacheValidity = std::chrono::seconds(7 * 24 * 60 * 60);
};
}}}}
