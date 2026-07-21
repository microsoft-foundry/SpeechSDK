//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "web_socket_delegate_helper.h"
#include "spxdebug.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <typename DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxWebSocket, true>>
class ISpxWebSocketDelegateImpl :
    protected CSpxWebSocketDelegateHelper<DelegateToHelperT>,
    public ISpxWebSocket
{
private:

    using D = CSpxWebSocketDelegateHelper<DelegateToHelperT>;

public:
    ~ISpxWebSocketDelegateImpl()
    {
        SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

        if (D::IsReady())
        {
            auto d = D::GetDelegate();

            d->OnBinaryData.Clear();
            d->OnConnected.Clear();
            d->OnDisconnected.Clear();
            d->OnError.Clear();
            d->OnEstimatedUploadRateKBPerSec.Clear();
            d->OnTextData.Clear();
        }
    };

    void Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId = "") override
    {
        D::DelegateConnect(webSocketEndpoint, connectionId);
    }

    void Disconnect() override
    {
        D::DelegateDisconnect();
    }

    void SendTextData(const std::string& text) override
    {
        D::DelegateSendTextData(text);
    }

    void SendBinaryData(const uint8_t* data, const size_t size) override
    {
        D::DelegateSendBinaryData(data, size);
    }

    void SendData(const std::shared_ptr<IWebSocketMessage>& message) override
    {
        D::DelegateSendData(message);
    }

    WebSocketState GetState() const override
    {
        return D::DelegateGetState();
    }

protected:
    void InitDelegatePtr(ISpxWebSocket::Ptr& ptr) override
    {
        SPX_DBG_ASSERT(nullptr != ptr);

        // Register the six callbacks on the underlying WebSocket using the
        // safe Event<>::Add(shared_ptr, &method) overload, which captures
        // the delegate-impl by weak_ptr and locks before each invocation.
        // This ensures callbacks cannot dereference a destroyed
        // delegate-impl, even if Raise has already snapshotted the handler
        // list at the moment destruction completes.
        //
        // See docs/architecture/core-architecture/lifetime-safety-invariants.md
        // invariant 2 (Event<>::Add registrations must use the safe overload).
        using SelfType = ISpxWebSocketDelegateImpl<DelegateToHelperT>;
        auto baseSelf = this->shared_from_this();
        auto self = std::static_pointer_cast<SelfType>(baseSelf);

        ptr->OnBinaryData.Add(self, &SelfType::OnBinaryDataCallback);
        ptr->OnConnected.Add(self, &SelfType::OnConnectedCallback);
        ptr->OnDisconnected.Add(self, &SelfType::OnDisconnectedCallback);
        ptr->OnError.Add(self, &SelfType::OnErrorCallback);
        ptr->OnEstimatedUploadRateKBPerSec.Add(self, &SelfType::OnEstimatedUploadRateKBPerSecCallback);
        ptr->OnTextData.Add(self, &SelfType::OnTextDataCallback);
    }

    virtual void OnBinaryDataCallback(const uint8_t* data, const size_t size)
    {
        this->OnBinaryData(data, size);
    }
    virtual void OnConnectedCallback(const std::string &url)
    {
        this->OnConnected(url);
    }
    virtual void OnDisconnectedCallback(WebSocketDisconnectReason reason, const std::string& error, bool serverRequested)
    {
        this->OnDisconnected(reason, error, serverRequested);
    }
    virtual void OnErrorCallback(const ISpxErrorInformation::Ptr& errorInfo)
    {
        this->OnError(errorInfo);
    }
    virtual void OnEstimatedUploadRateKBPerSecCallback(const float rate)
    {
        this->OnEstimatedUploadRateKBPerSec(rate);
    }
    virtual void OnTextDataCallback(const std::string& data)
    {
        this->OnTextData(data);
    }
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
