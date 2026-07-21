//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <stdexcept>
#include "web_socket.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    static constexpr auto NO_NETWORKING = "Native networking is disabled";

    void CSpxWebSocket::Connect(const IHttpEndpointInfo& endpoint, const std::string& connectionId)
    {
        (void)endpoint;
        (void)connectionId;
        throw std::runtime_error(NO_NETWORKING);
    }

    void CSpxWebSocket::Disconnect()
    {
    }

    void CSpxWebSocket::SendTextData(const std::string& text)
    {
        (void)text;
        throw std::runtime_error(NO_NETWORKING);
    }

    void CSpxWebSocket::SendBinaryData(const uint8_t* data, const size_t size)
    {
        (void)data;
        (void)size;
        throw std::runtime_error(NO_NETWORKING);
    }

    void CSpxWebSocket::SendData(const std::shared_ptr<IWebSocketMessage>& message)
    {
        (void)message;
        throw std::runtime_error(NO_NETWORKING);
    }

    WebSocketState CSpxWebSocket::GetState() const
    {
        return WebSocketState::INITIAL;
    }

}}}}
