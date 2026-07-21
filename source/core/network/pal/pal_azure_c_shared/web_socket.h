//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// web_socket.h: A web socket C++ implementation using the Azure C shared library
//

#pragma once

#include <queue>
#include <mutex>
#include <thread>
#include <ispxinterfaces.h>
#include <interfaces/web_socket.h>
#include <interfaces/i_web_socket_init.h>
#include <interfaces/ISpxNetworkPlatformInit.h>
#include <i_telemetry.h>
#include <no_op_telemetry.h>
#include <interface_helpers.h>
#include <queued_item.h>
#include "azure_c_shared_utility_uws_client_wrapper.h"

namespace WebSocketAdapter
{
    class IWebSocketAdapter;
}

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class CSpxWebSocket :
        public ISpxWebSocket,
        public ISpxWebSocketInit,
        public ISpxInterfaceBaseFor<CSpxWebSocket>,
        public ISpxNetworkPlatformInit
    {
    public:
        CSpxWebSocket();
        virtual ~CSpxWebSocket();

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(ISpxWebSocket)
            SPX_INTERFACE_MAP_ENTRY(ISpxWebSocketInit)
            SPX_INTERFACE_MAP_ENTRY(ISpxNetworkPlatformInit)
        SPX_INTERFACE_MAP_END()

        virtual void Init(
            const std::shared_ptr<ISpxThreadService>& threadService,
            const ISpxThreadService::Affinity affinity,
            const std::chrono::milliseconds& pollingIntervalMs,
            const std::shared_ptr<USP::ISpxWebSocketTelemetry>& telemetry,
            const std::shared_ptr<ISpxHttpErrorHandler>& httpErrorHandler = nullptr) override;

        virtual void SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs) override;

        virtual void Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId) override;
        virtual void Disconnect() override;

        virtual void SendTextData(const std::string& text) override;
        virtual void SendBinaryData(const uint8_t *data, const size_t size) override;
        virtual void SendData(const std::shared_ptr<IWebSocketMessage>& message) override;

        virtual WebSocketState GetState() const override { return m_state.load(); }

        virtual std::function<void()> Init() override;

        Event<WebSocketState, WebSocketState> OnStateChanged;

    protected:
        void ChangeState(WebSocketState state)
        {
            WebSocketState previous = m_state.exchange(state);
            if (previous != state)
            {
                HandleWebSocketStateChanged(previous, state);
            }
        }

        void ChangeState(WebSocketState from, WebSocketState next)
        {
            WebSocketState compare = from;
            bool success = m_state.compare_exchange_strong(compare, next);
            if (!success)
            {
                // for debugging
                SPX_TRACE_ERROR("Failed to change state for WebSocket %p. From: %d, To: %d, Current: %d",
                    static_cast<void*>(this), static_cast<int>(from), static_cast<int>(next), static_cast<int>(compare));

                // force it anyway
                from = m_state.exchange(next);
            }

            HandleWebSocketStateChanged(from, next);
        }

        virtual int Connect();

        virtual void QueueMessage(const std::shared_ptr<IWebSocketMessage>& message);
        virtual int SendMessage(OutgoingQueuedItem&& item);

        virtual void HandleConnected(const std::string &url);
        virtual void HandleDisconnected(WebSocketDisconnectReason reason, const std::string& cause, bool serverRequested);
        virtual void HandleTextData(const std::string& data);
        virtual void HandleBinaryData(const uint8_t* data, const size_t size);
        virtual void HandleError(WebSocketError reason, int errorCode, const std::string& errorMessage);
        virtual void HandleWebSocketStateChanged(WebSocketState oldState, WebSocketState newState);
        virtual void HandleWebSocketFrameSent(OutgoingQueuedItem& message, WS_SEND_FRAME_RESULT result);

    private:
        DISABLE_COPY_AND_MOVE(CSpxWebSocket);

        static void WorkLoop(std::weak_ptr<CSpxWebSocket> ptr);
        void DoWork();

        /// <summary>
        /// Performs the actual native teardown of m_webSocket. Called by
        /// Disconnect() while holding m_workMutex, which guarantees that no
        /// DoWork() invocation is currently executing on this instance and
        /// none can start until teardown completes.
        /// </summary>
        void PerformTeardown();

        // Serializes DoWork() against Disconnect() / teardown, so that an
        // in-flight DoWork on a worker thread (including a thread service
        // worker that has been Term()'d-then-detached and is still wedged
        // inside socket I/O) cannot race a teardown from a disposing thread.
        // Recursive because HandleError() called from inside DoWork() can
        // reach Disconnect() via the error callback chain
        // (HandleError -> OnError -> ResetWebSocket -> ~CSpxWebSocket ->
        // Disconnect), which must be allowed to re-enter the lock from the
        // same thread.
        std::recursive_mutex m_workMutex;

        void OnWebSocketOpened(WS_OPEN_RESULT_DETAILED open_result_detailed);
        void OnWebSocketFrameReceived(unsigned char frame_type, const unsigned char* buffer, size_t size);
        void OnWebSocketPeerClosed(uint16_t* closeCode, const unsigned char* extraData, size_t extraDataLength);
        void OnWebSocketError(WS_ERROR errorCode);
        void OnWebSocketClosed();

        uint64_t GetTimestamp() const;

#define _EX_FIRST_ "%s thrown in a callback from Azure C Shared and will be re-thrown in C++ code. Details: %s"
#define _EX_SUBSEQUENT_ "An exception from a previous Azure C Shared callback was not yet handled. The following will be ignored: %s"

        template<typename ...Args>
        static inline void cast_and_invoke(void* context, void (CSpxWebSocket::* callback)(Args...), Args... args)
        {
            try
            {
                auto webSocket = static_cast<CSpxWebSocket*>(context);
                if (webSocket != nullptr)
                {
                    try
                    {
                        (webSocket->*callback)(args...);
                    }
                    catch (const std::exception& ex)
                    {
                        if (!webSocket->m_exceptionAcrossCBoundary)
                        {
                            webSocket->m_exceptionAcrossCBoundary = std::current_exception();
                            SPX_TRACE_ERROR(_EX_FIRST_, "An exception", ex.what());
                        }
                        else
                        {
                            SPX_TRACE_ERROR(_EX_SUBSEQUENT_, ex.what());
                        }
                    }
                    catch (AZACHR hr)
                    {
                        if (!webSocket->m_exceptionAcrossCBoundary)
                        {
                            webSocket->m_exceptionAcrossCBoundary = std::make_exception_ptr(ExceptionWithCallStack(hr));
                            SPX_TRACE_ERROR(_EX_FIRST_, "A HR code", stringify(hr).c_str());
                        }
                        else
                        {
                            SPX_TRACE_ERROR(_EX_SUBSEQUENT_, stringify(hr).c_str());
                        }
                    }
                    catch (...)
                    {
                        if (!webSocket->m_exceptionAcrossCBoundary)
                        {
                            webSocket->m_exceptionAcrossCBoundary = std::make_exception_ptr(ExceptionWithCallStack("Unknown exception in C++ callback from Azure C Shared code"));
                            SPX_TRACE_ERROR(_EX_FIRST_, "An error", "unknown error");
                        }
                        else
                        {
                            SPX_TRACE_ERROR(_EX_SUBSEQUENT_, "unknown error");
                        }
                    }
                }
            }
            catch (const std::exception& ex)
            {
                SPX_TRACE_ERROR("Exception in casting context to an instance of CSpxWebSocket. What: %s", ex.what());
            }
            catch (AZACHR hr)
            {
                SPX_TRACE_ERROR("Error in casting context to an instance of CSpxWebSocket. HR: %p (%" PRIuPTR ")", (void*)hr, hr);
            }
            catch (...)
            {
                SPX_TRACE_ERROR("Error in casting context to an instance of CSpxWebSocket");
            }
        }

    protected:
        std::atomic_bool m_valid;
        std::atomic_bool m_open;
        std::shared_ptr<USP::ISpxWebSocketTelemetry> m_telemetry;
        std::string m_connectionId;
        std::chrono::steady_clock::time_point m_ratePeriodEnds;
        double m_bytesSentInPeriod;
        double m_avgUploadRateKBPerSec;
        size_t m_numUploadRateSamples;

    private:
        std::shared_ptr<ISpxThreadService> m_threadService;
        ISpxThreadService::Affinity m_affinity;
        std::chrono::milliseconds m_pollingIntervalMs;
        uint64_t m_creationTime;
        uint64_t m_connectionTime;

        std::map<std::string, int> m_webSocketUnderlyingOptions;
        std::shared_ptr<WebSocketAdapter::IWebSocketAdapter> m_webSocket;
        std::atomic<WebSocketState> m_state;
        std::queue<OutgoingQueuedItem> m_queue;
        std::mutex m_queue_lock;
        std::exception_ptr m_exceptionAcrossCBoundary;
        std::unique_ptr<IHttpEndpointInfo> m_request;
        ISpxHttpErrorHandler::Ptr m_httpErrorHandler;
    };

} } } }
