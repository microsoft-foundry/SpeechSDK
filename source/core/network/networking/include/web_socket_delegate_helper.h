//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "../stdafx.h"
#include "interface_delegate_helpers.h"
#include "ispxinterfaces.h"
#include "interfaces/web_socket.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxWebSocket>>
class CSpxWebSocketDelegateHelper :
    public DelegateToHelperT
{
private:

    using I = ISpxWebSocket;
    using C = CSpxWebSocketDelegateHelper<DelegateToHelperT>;

public:

    SPX_DELEGATE_ACCESSORS(CSpxWebSocket, DelegateToHelperT, ISpxWebSocket);

    void DelegateConnect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId = "")
    {
        InvokeOnDelegate(C::GetDelegate(), &I::Connect, webSocketEndpoint, connectionId);
    }

    void DelegateDisconnect()
    {
        InvokeOnDelegate(C::GetDelegate(), &I::Disconnect);
    }

    void DelegateSendTextData(const std::string& text)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SendTextData, text);
    }

    void DelegateSendBinaryData(const uint8_t* data, const size_t size)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SendBinaryData, data, size);
    }

    void DelegateSendData(const std::shared_ptr<IWebSocketMessage>& message)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SendData, message);
    }

    WebSocketState DelegateGetState() const
    {
        return InvokeOnDelegateR(C::GetConstDelegate(), &I::GetState, WebSocketState::INITIAL);
    }
};
}}}} // Microsoft::CognitiveServices::Speech::Impl
