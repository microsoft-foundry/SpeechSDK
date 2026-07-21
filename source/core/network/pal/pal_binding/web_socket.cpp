//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "web_socket.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

void CSpxBindingBasedWebSocket::Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId)
{
    // Implementation would go here
    UNUSED(webSocketEndpoint);
    UNUSED(connectionId);
}

void CSpxBindingBasedWebSocket::Disconnect()
{
    // Implementation would go here
}

void CSpxBindingBasedWebSocket::SendTextData(const std::string& text)
{
    // Implementation would go here
    UNUSED(text);
}

void CSpxBindingBasedWebSocket::SendBinaryData(const uint8_t* data, const size_t size)
{
    // Implementation would go here
    UNUSED(data);
    UNUSED(size);
}

void CSpxBindingBasedWebSocket::SendData(const std::shared_ptr<IWebSocketMessage>& message)
{
    // Implementation would go here
    UNUSED(message);
}

WebSocketState CSpxBindingBasedWebSocket::GetState() const
{
    // Implementation would go here
    return WebSocketState::CLOSED;
}

}}}}
