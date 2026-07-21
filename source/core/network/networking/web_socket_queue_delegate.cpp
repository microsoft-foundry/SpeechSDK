//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "include/web_socket_queue_delegate.h"
#include "create_object_helpers.h"
#include "site_helpers.h"
#include "web_socket_message.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech{
namespace Impl {

void CSpxQueuingWebSocket::SendTextData(const std::string& text)
{
    if (GetState() == WebSocketState::CONNECTED)
    {
        WS_Child::SendTextData(text);
    }
    else
    {
        QueueMessage(std::make_shared<WebSocketMessage>(text));
    }
}

void CSpxQueuingWebSocket::SendBinaryData(const uint8_t* data, const size_t size)
{
    if (GetState() == WebSocketState::CONNECTED)
    {
        WS_Child::SendBinaryData(data, size);
    }
    else
    {
        QueueMessage(std::make_shared<WebSocketMessage>(data, size));
    }
}

void CSpxQueuingWebSocket::SendData(const IWebSocketMessage::Ptr& message)
{
    if (GetState() == WebSocketState::CONNECTED)
    {
        WS_Child::SendData(message);
    }
    else
    {
        QueueMessage(message);
    }
}

void CSpxQueuingWebSocket::InitDelegatePtr(ISpxWebSocket::Ptr& targetWebSocket)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    targetWebSocket = SpxCreateObjectWithSite<ISpxWebSocket>("CSpxRedirectingWebSocket", GetSite());
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

void CSpxQueuingWebSocket::InitDelegatePtr(ISpxWebSocketInit::Ptr& ptr)
{
    ptr = SpxQueryInterface<ISpxWebSocketInit>(CSpxWebSocketDelegateHelper::GetDelegate());
}

void CSpxQueuingWebSocket::QueueMessage(const IWebSocketMessage::Ptr packet)
{
    SPX_TRACE_INFO("Queuing message until socket is open");

    if (GetState() == WebSocketState::CLOSED)
    {
        SPX_TRACE_ERROR("Trying to send on a closed socket");
        
        throw ExceptionWithCallStack("Web socket is not open", SPXERR_INVALID_STATE);
    }

     m_queue.push(std::move(packet));
}

void CSpxQueuingWebSocket::OnConnectedCallback(const std::string& url)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_TRACE_INFO("Connected, forwarding %zu queued messages", m_queue.size());
    while (!m_queue.empty())
    {
        WS_Child::SendData(std::move(m_queue.front()));
        m_queue.pop();
    }

    WS_Child::OnConnectedCallback(url);
}

}}}}
