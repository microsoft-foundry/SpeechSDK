//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <queue>
#include "web_socket_delegate_impl.h"
#include "web_socketInit_delegate_impl.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxQueuingWebSocket :
    public ISpxWebSocketDelegateImpl<>,
    public ISpxWebSocketInitDelegateImpl<>,
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>
{
private:
    using WS_Child = ISpxWebSocketDelegateImpl<CSpxDelegateToSharedPtrHelper<ISpxWebSocket, true>>;

public:
    CSpxQueuingWebSocket() = default;
    ~CSpxQueuingWebSocket() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        using namespace USP;
        SPX_INTERFACE_MAP_ENTRY(ISpxWebSocket)
        SPX_INTERFACE_MAP_ENTRY(ISpxWebSocketInit)
    SPX_INTERFACE_MAP_END()

    void SendTextData(const std::string& text) override;
    void SendBinaryData(const uint8_t* data, const size_t size) override;
    void SendData(const std::shared_ptr<IWebSocketMessage>& message) override;

protected:
    void InitDelegatePtr(ISpxWebSocket::Ptr &targetWebSocket) override;
    void InitDelegatePtr(ISpxWebSocketInit::Ptr& ptr) override;
    void OnConnectedCallback(const std::string&) override;

private:
    std::queue<IWebSocketMessage::Ptr> m_queue;
    virtual void QueueMessage(const IWebSocketMessage::Ptr message);

};
}}}}
