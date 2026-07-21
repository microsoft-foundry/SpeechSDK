//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>

#include <interfaces/thread_service.h>
#include <interfaces/web_socket.h>
#include <interfaces/i_web_socket_init.h>

#include <interface_helpers.h>
#include <time_utils.h>
#include <queued_item.h>

#include <azure/core/http/websockets/websockets.hpp>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Web socket implementation using Azure core web sockets
    /// </summary>
    class CSpxWebSocket :
        public ISpxWebSocket,
        public ISpxWebSocketInit
    {
    private:
        ISpxThreadService::Ptr m_threadService;
        ISpxThreadService::Affinity m_affinity;
        std::chrono::milliseconds m_pollingInterval;
        std::shared_ptr<ISpxWebSocketTelemetry> m_telemetry;
        ISpxHttpErrorHandler::Ptr m_errorHandler;

        std::atomic<WebSocketState> m_state;
        std::atomic<bool> m_inited;

        std::unique_ptr<IHttpEndpointInfo> m_endpointInfo;
        Azure::Core::Context m_context;
        std::unique_ptr<Azure::Core::Http::WebSockets::_internal::WebSocket> m_webSocket;
        std::thread m_receiveThread;

        std::mutex m_incomingLock;
        std::queue<IncomingQueuedItem> m_incomingQueue;
        std::mutex m_outgoingLock;
        std::queue<OutgoingQueuedItem> m_outgoingQueue;

    public:
        CSpxWebSocket();
        ~CSpxWebSocket();

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(ISpxWebSocket)
            SPX_INTERFACE_MAP_ENTRY(ISpxWebSocketInit)
        SPX_INTERFACE_MAP_END()

        virtual void Init(
            const ISpxThreadService::Ptr& threadService,
            const ISpxThreadService::Affinity affinity,
            const std::chrono::milliseconds& pollingIntervalMs,
            const std::shared_ptr<ISpxWebSocketTelemetry>& telemetry,
            const ISpxHttpErrorHandler::Ptr& httpErrorHandler = nullptr) override;

        virtual void SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs) override;

        virtual void Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId = "") override;
        virtual void Disconnect() override;
        virtual void SendTextData(const std::string& text) override;
        virtual void SendBinaryData(const uint8_t* data, const size_t size) override;
        virtual void SendData(const std::shared_ptr<IWebSocketMessage>& message) override;
        virtual WebSocketState GetState() const override;

    protected:
        virtual void SendConnectedEvent(const std::string& url);
        virtual void SendDisconnectedEvent(WebSocketDisconnectReason reason, const std::string& message, bool serverRequested);
        virtual void SendTextDataEvent(const std::string& data);
        virtual void SendBinaryDataEvent(const uint8_t* data, const size_t size);
        virtual void SendErrorEvent(const ISpxErrorInformation::Ptr& error);
        virtual void SendEsitmatedUploadRateKBPerSecEvent(const float uploadRateKBperSec);

    private:
        void ChangeState(WebSocketState from, WebSocketState to);
        void QueueMessage(const std::shared_ptr<IWebSocketMessage>& packet);

        void DoWork();
        void DoConnect();
        void DoReceive(const size_t max);
        void DoSend(const size_t max);

        void ReceiveThread();

        void HandleFatalError(WebSocketError error, int32_t errorCode, const std::string& details = {});

        void RunAsync(std::function<void()> func, const std::chrono::milliseconds& delay = std::chrono::milliseconds(0));
        void RunSync(std::function<void()> func);
    };

}}}}
