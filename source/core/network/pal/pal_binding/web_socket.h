//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <interfaces/i_web_socket_init.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxBindingBasedWebSocket : public ISpxWebSocket
{
public:
    CSpxBindingBasedWebSocket() = default;
    virtual ~CSpxBindingBasedWebSocket() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxWebSocket)
    SPX_INTERFACE_MAP_END()

    virtual void Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId) override;
    virtual void Disconnect() override;

    void SendTextData(const std::string& text) override;
    void SendBinaryData(const uint8_t* data, const size_t size) override;
    void SendData(const std::shared_ptr<IWebSocketMessage>& message) override;

    virtual WebSocketState GetState() const override;
};

}}}}
