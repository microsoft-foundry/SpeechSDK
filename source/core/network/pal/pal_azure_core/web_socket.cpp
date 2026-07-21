//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <array>
#include <sstream>

#include "web_socket.h"
#include "i_telemetry.h"
#include "no_op_telemetry.h"
#include "default_http_error_handler.h"
#include "string_utils.h"
#include "web_socket_enum_helpers.h"
#include "error_info.h"
#include "web_socket_message.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    using namespace std::chrono_literals;
    using Lock = std::lock_guard<std::mutex>;

    namespace core = Azure::Core;
    namespace http = Azure::Core::Http;
    namespace ws = Azure::Core::Http::WebSockets;
    namespace ws_detail = Azure::Core::Http::WebSockets::_detail;
    namespace ws_internal = Azure::Core::Http::WebSockets::_internal;

    // use this assumption in several places
    static_assert(sizeof(char) == sizeof(uint8_t), "Size of char is not equal to size of uint8_t on your platform");

    static const std::string FAILED_TO_SEND_MSG{ "Failed while sending web socket message" };
    static const std::string FAILED_TO_SEND_DETAILS{ FAILED_TO_SEND_MSG + ". Details: " };
    static constexpr auto FAILED_TO_SEND_ERR = SPXERR_NETWORK_SEND_FAILED;

    // ====================================================================
    // Helper types
    // ====================================================================
    enum class InternalWebSocketError : int
    {
        None = 0,
        Unknown = -1,
        ConnectTransport = -10,
        ConnectError = -11,
        WorkerThread = -20,
        ReceiveThreadTransport = -30,
        ReceiveThreadError = -31,
        ReceiveWorkerError = -32,
        SendWorkerTransport = -40,
        SendWorkerError = -41,
    };

    class ResponseWrapper : public ISpxHttpResponse
    {
    private:
        http::RawResponse* m_response;
        IHttpEndpointInfo* m_endpoint;
        ISpxHttpErrorHandler* m_errorHandler;

    public:
        ResponseWrapper(http::RawResponse* response, IHttpEndpointInfo* endpoint, ISpxHttpErrorHandler* errorHandler) :
            m_response(response),
            m_endpoint(endpoint),
            m_errorHandler(errorHandler)
        {
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, response == nullptr);
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, endpoint == nullptr);
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, errorHandler == nullptr);
        }

        virtual bool IsSuccess() const override
        {
            return m_errorHandler->IsSuccess(this);
        }

        virtual void EnsureSuccess() const override
        {
            m_errorHandler->HandleResponse(HttpMethod::Get, m_endpoint, this);
        }

        virtual unsigned int GetStatusCode() const override
        {
            return static_cast<unsigned int>(m_response->GetStatusCode());
        }

        virtual std::string GetReasonPhrase() const override
        {
            return m_response->GetReasonPhrase();
        }

        virtual std::string GetHeader(const std::string& name) const override
        {
            const auto& headers = m_response->GetHeaders();
            auto found = headers.find(name);
            return found == headers.end()
                ? std::string{}
                : found->second;
        }

        virtual std::string ReadContentAsString(const size_t maxLength = (std::numeric_limits<size_t>::max)()) const override
        {
            Azure::DateTime deadline(std::chrono::system_clock::now() + 5s);
            auto context = (core::Context{}).WithDeadline(deadline);

            auto stream = m_response->ExtractBodyStream();
            if (stream == nullptr)
            {
                return {};
            }

            // sometimes we don't know the length of the body, and we may get back 0 or -1. So we can't really
            // trust is unless we try to read. Use a string stream to work around this which does result in
            // an extra copy here but this is simplest way and this is not a critical path
            std::ostringstream oss;
            std::array<uint8_t, 256> buffer;
            size_t totalRead = 0;

            try
            {
                while (totalRead < maxLength)
                {
                    size_t numRead = stream->ReadToCount(buffer.data(), std::min(buffer.size(), maxLength - totalRead), context);
                    totalRead += numRead;
                    if (numRead == 0)
                    {
                        break;
                    }

                    oss.write(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(numRead));
                }
            }
            catch (const std::exception& ex)
            {
                // TODO handle
                oss << "\n EXCEPTION: " << ex.what();
            }

            return oss.str();
        }
    };



    // ====================================================================
    // Helper methods
    // ====================================================================
    static bool IsOpen(WebSocketState state)
    {
        switch (state)
        {
        case WebSocketState::OPENING:
        case WebSocketState::CONNECTED:
            return true;

        case WebSocketState::INITIAL:
        case WebSocketState::DESTROYING:
        case WebSocketState::CLOSED:
            return false;
        }

        return false; // should never get here
    }

    static void ResizeBuffer(std::vector<uint8_t>& buffer, size_t additionalBytes, const size_t maxSize)
    {
        size_t totalSize = buffer.size() + additionalBytes;
        if (totalSize > maxSize)
        {
            throw ExceptionWithCallStack("Incoming web socket Frame is too large to fit into buffer", SPXERR_BUFFER_TOO_SMALL);
        }
        // TODO is this needed? Will the vector make sure we don't resize multiple times on its own for typical
        //      message sizes?
        else if (totalSize > buffer.capacity())
        {
            size_t newSize = std::min(maxSize, static_cast<size_t>(totalSize * 1.3));
            buffer.reserve(newSize);
        }
    }

    static void ApppendToBuffer(std::vector<uint8_t>& buffer, const std::string& text, const size_t maxSize)
    {
        ResizeBuffer(buffer, text.length(), maxSize);

        const char* ptr = text.c_str();
        for (size_t i = 0; i < text.length(); i++)
        {
            buffer.push_back(static_cast<uint8_t>(*ptr++));
        }
    }

    static void ApppendToBuffer(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& data, const size_t maxSize)
    {
        ResizeBuffer(buffer, data.size(), maxSize);
        buffer.insert(buffer.end(), data.begin(), data.end());
    }

    static WebSocketFrameType ToFrameType(Azure::Core::Http::WebSockets::_internal::WebSocketFrameType type)
    {
        switch (type)
        {
        case ws_internal::WebSocketFrameType::BinaryFrameReceived:  return WebSocketFrameType::Binary;
        case ws_internal::WebSocketFrameType::PeerClosedReceived:   return WebSocketFrameType::Close;
        case ws_internal::WebSocketFrameType::TextFrameReceived:    return WebSocketFrameType::Text;
        case ws_internal::WebSocketFrameType::Unknown:              return WebSocketFrameType::Unknown;
        }

        // should never get here
        return WebSocketFrameType::Unknown;
    }



    // ====================================================================
    // CSpxWebSocket
    // ====================================================================
    CSpxWebSocket::CSpxWebSocket() :
        m_threadService(nullptr),
        m_affinity(ISpxThreadService::Affinity::Background),
        m_pollingInterval(100ms),
        m_telemetry(USP::NoOpTelemetry::Instance()),
        m_errorHandler(GetDefaultHttpErrorHandler()),
        m_state(WebSocketState::INITIAL),
        m_inited(false),
        m_endpointInfo(),
        m_context(),
        m_webSocket(nullptr),
        m_receiveThread(),
        m_incomingLock(),
        m_incomingQueue(),
        m_outgoingLock(),
        m_outgoingQueue()
    {
    }

    CSpxWebSocket::~CSpxWebSocket()
    {
        SPX_TRACE_VERBOSE("[0x%p] Web socket destructor called", (void*)this);

        auto state = m_state.load(std::memory_order_relaxed);
        switch (state)
        {
        case WebSocketState::INITIAL:
        case WebSocketState::OPENING:
        case WebSocketState::CONNECTED:
            ChangeState(state, WebSocketState::DESTROYING);
            if (m_webSocket)
            {
                // NOTE: There is an issue right now that causes the web socket destructor
                //       to call std::abort because the ping/pong thread is not properly
                //       terminated. As a work around, let's call close here for now

                // NOTE: This can throw exceptions if the web socket is not open yet
                try
                {
                    m_webSocket->Close();
                }
                catch (...)
                {
                    // ignore for now
                }
            }
            break;

        case WebSocketState::DESTROYING:
        case WebSocketState::CLOSED:
            // nothing extra needed here
            break;
        }

        if (m_receiveThread.joinable())
        {
            m_receiveThread.join();
        }

        m_webSocket.reset();

        ChangeState(state, WebSocketState::CLOSED);

        SPX_TRACE_VERBOSE("[0x%p] Web socket destructor finished", (void*)this);
    }

    void CSpxWebSocket::Init(
        const ISpxThreadService::Ptr& threadService,
        const ISpxThreadService::Affinity affinity,
        const std::chrono::milliseconds& pollingIntervalMs,
        const ISpxWebSocketTelemetry::Ptr& telemetry,
        const ISpxHttpErrorHandler::Ptr& httpErrorHandler)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, threadService == nullptr);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, pollingIntervalMs <= 0ms);

        bool expected = false;
        bool success = m_inited.compare_exchange_strong(expected, true, std::memory_order_release, std::memory_order_relaxed);
        SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !success);

        m_threadService = threadService;
        m_affinity = affinity;
        m_pollingInterval = pollingIntervalMs;

        if (telemetry)
        {
            m_telemetry = telemetry;
        }

        if (httpErrorHandler)
        {
            m_errorHandler = httpErrorHandler;
        }
    }

    void CSpxWebSocket::SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs)
    {
        // Validate the polling interval is positive and reasonable
        if (pollingIntervalMs.count() <= 0)
        {
            SPX_TRACE_WARNING("%s: Invalid polling interval %lld ms (must be > 0), ignoring",
                __FUNCTION__, static_cast<long long>(pollingIntervalMs.count()));
            return;
        }
        constexpr long long kMaxPollingMs = 60000;
        if (pollingIntervalMs.count() > kMaxPollingMs)
        {
            SPX_TRACE_WARNING("%s: Polling interval %lld ms exceeds maximum (%lld ms), clamping",
                __FUNCTION__, static_cast<long long>(pollingIntervalMs.count()), kMaxPollingMs);
            m_pollingInterval = std::chrono::milliseconds(kMaxPollingMs);
            return;
        }

        SPX_DBG_TRACE_VERBOSE("%s: Updating polling interval from %lld ms to %lld ms",
            __FUNCTION__,
            static_cast<long long>(m_pollingInterval.count()),
            static_cast<long long>(pollingIntervalMs.count()));
        m_pollingInterval = pollingIntervalMs;
    }

    void CSpxWebSocket::Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !webSocketEndpoint.IsValid());

        // TODO what should I do with the connection ID??
        UNUSED(connectionId);

        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_inited.load(std::memory_order_acquire) == false);

        WebSocketState currentState = WebSocketState::INITIAL;
        auto changedState = m_state.compare_exchange_strong(
            currentState, WebSocketState::OPENING, std::memory_order_release, std::memory_order_relaxed);
        if (!changedState)
        {
            SPX_TRACE_WARNING("[0x%p] Web socket not in valid state to connect: '%s'", (void*)this, EnumHelpers::ToString(currentState));
            SPX_THROW_HR_IF(SPXERR_INVALID_STATE, !changedState);
        }

        // TODO raise state changed event

        m_endpointInfo = webSocketEndpoint.Clone();

        ws_internal::WebSocketOptions options;

        auto proxy = webSocketEndpoint.Proxy();
        if (!proxy.host.empty())
        {
            std::string proxyPort(proxy.host);
            if (proxy.port > 0)
            {
                proxyPort.append(":");
                proxyPort.append(std::to_string(proxy.port));
            }

            options.Transport.HttpProxy = proxyPort;
        }

        if (webSocketEndpoint.WebSocketProtocolsSize() > 0)
        {
            auto wsProtocols = PAL::StringUtils::Tokenize(webSocketEndpoint.WebSocketProtocols(), ", ");
            options.Protocols.insert(options.Protocols.end(), wsProtocols.begin(), wsProtocols.end());
        }

        // TODO figure out what to do with the fun SSL stuff

        core::Url url(webSocketEndpoint.EndpointUrl());
        m_webSocket = std::make_unique<ws_internal::WebSocket>(url, options);

        for (const auto& header : webSocketEndpoint.Headers())
        {
            m_webSocket->AddHeader(header.first, header.second);
        }

        // TODO the connect method is synchronous. For now "schedule" the actual connect for later
        //      by running the DoWork()
        RunAsync([this]() { DoWork(); });
    }

    void CSpxWebSocket::Disconnect()
    {
        // NOTE: To prevent race conditions, we'll run the code this code synchronously on the background thread
        RunSync([this]()
        {
            const WebSocketDisconnectReason reason{ WebSocketDisconnectReason::Normal };
            const std::string msg;

            auto state = m_state.load(std::memory_order_acquire);
            switch (state)
            {
            case WebSocketState::DESTROYING:
                // Azure core has synchronous operations, and we are running on single thread. Thus this should be impossible
                return;

            case WebSocketState::INITIAL:
            case WebSocketState::CLOSED:
                // nothing to do
                return;

            default:
                // fall through
                break;
            }

            SPX_TRACE_INFO("[0x%p] Web socket connection closing (user requested). State: %s",
                (void*)this, EnumHelpers::ToString(state));

            ChangeState(state, WebSocketState::DESTROYING);

            try
            {
                if (m_webSocket)
                {
                    m_webSocket->Close(
                        static_cast<uint16_t>(reason), msg, m_context);
                }

                SPX_TRACE_INFO("[0x%p] Web socket successfully closed", (void*)this);
            }
            catch (const std::exception& ex)
            {
                SPX_TRACE_ERROR("[0x%p] Web socket close failed. Details: %s", (void*)this, ex.what());
            }
            catch (...)
            {
                SPX_TRACE_ERROR("[0x%p] Web socket close failed", (void*)this);
            }

            ChangeState(WebSocketState::DESTROYING, WebSocketState::CLOSED);
            // TODO telemetry: MetricsTransportClosed();

            SendDisconnectedEvent(reason, msg, false);
        });
    }

    void CSpxWebSocket::SendTextData(const std::string& text)
    {
        if (text.empty())
        {
            return;
        }

        QueueMessage(std::make_shared<WebSocketMessage>(text));
    }

    void CSpxWebSocket::SendBinaryData(const uint8_t* data, const size_t size)
    {
        if (data == nullptr || size == 0)
        {
            return;
        }

        QueueMessage(std::make_shared<WebSocketMessage>(data, size));
    }

    void CSpxWebSocket::SendData(const std::shared_ptr<IWebSocketMessage>& message)
    {
        if (message == nullptr)
        {
            return;
        }

        QueueMessage(message);
    }

    WebSocketState CSpxWebSocket::GetState() const
    {
        return m_state.load(std::memory_order_acquire);
    }

    template<typename TEvent, typename... TArgs>
    static void RaiseEvent(void* instance, TEvent& ev, TArgs&&... args)
    {
        try
        {
            ev.Raise(std::forward<TArgs>(args)...);
        }
        catch (const std::exception& ex)
        {
            SPX_TRACE_WARNING("[0x%p] Web socket event handler exception will be ignored. Details: %s", instance, ex.what());
        }
        catch (...)
        {
            SPX_TRACE_WARNING("[0x%p] Web socket event handler exception will be ignored", instance);
        }
    }

    void CSpxWebSocket::SendConnectedEvent(const std::string& url)
    {
        RaiseEvent(this, OnConnected, url);
    }

    void CSpxWebSocket::SendDisconnectedEvent(WebSocketDisconnectReason reason, const std::string& message, bool serverRequested)
    {
        RaiseEvent(this, OnDisconnected, reason, message, serverRequested);
    }

    void CSpxWebSocket::SendTextDataEvent(const std::string& data)
    {
        RaiseEvent(this, OnTextData, data);
    }

    void CSpxWebSocket::SendBinaryDataEvent(const uint8_t * data, const size_t size)
    {
        RaiseEvent(this, OnBinaryData, data, size);
    }

    void CSpxWebSocket::SendErrorEvent(const ISpxErrorInformation::Ptr & error)
    {
        RaiseEvent(this, OnError, error);
    }

    void CSpxWebSocket::SendEsitmatedUploadRateKBPerSecEvent(const float uploadRateKBperSec)
    {
        RaiseEvent(this, OnEstimatedUploadRateKBPerSec, uploadRateKBperSec);
    }

    void CSpxWebSocket::ChangeState(WebSocketState from, WebSocketState to)
    {
        WebSocketState prev = m_state.exchange(to, std::memory_order_release);
        if (prev != from)
        {
            SPX_TRACE_WARNING("[0x%p] Web socket state change from unexpected state. Actual: '%s', Expected: '%s', Next: '%s'",
                (void*)this, EnumHelpers::ToString(prev), EnumHelpers::ToString(from), EnumHelpers::ToString(to));
        }

        // TODO raise state changed event?
    }

    void CSpxWebSocket::QueueMessage(const std::shared_ptr<IWebSocketMessage>& packet)
    {
        auto state = m_state.load(std::memory_order_acquire);
        if (!IsOpen(state))
        {
            SPX_TRACE_ERROR("[0x%p] Web socket trying to send when in invalid state '%s'", (void*)this, EnumHelpers::ToString(state));
            throw ExceptionWithCallStack("Web socket is not open", SPXERR_INVALID_STATE);
        }

        OutgoingQueuedItem queued(packet);
        {
            Lock lock(m_outgoingLock);
            m_outgoingQueue.push(std::move(queued));
        }
    }

    void CSpxWebSocket::DoWork()
    {
        // TODO make these configurable
        const size_t MAX_SEND_PER_CYCLE = 20;
        const size_t MAX_RECEIVE_PER_CYCLE = 20;

        bool loop = true;

        auto state = m_state.load(std::memory_order_acquire);
        switch (state)
        {
        case WebSocketState::INITIAL:
            SPX_TRACE_ERROR("[0x%p] Web socket called do work in invalid state '%s'", (void*)this, EnumHelpers::ToString(state));
            loop = false;
            break;

        case WebSocketState::OPENING:
            DoConnect();
            break;

        case WebSocketState::CONNECTED:
            DoReceive(MAX_RECEIVE_PER_CYCLE);

            // we may have moved to a closed, or an error state
            if (m_state.load(std::memory_order_acquire) == WebSocketState::CONNECTED)
            {
                DoSend(MAX_SEND_PER_CYCLE);
            }
            else
            {
                loop = false;
            }
            break;

        case WebSocketState::DESTROYING:
        case WebSocketState::CLOSED:
            SPX_TRACE_VERBOSE("[0x%p] Web socket is now %s. Worker thread exiting", (void*)this, EnumHelpers::ToString(state));
            loop = false;
            break;
        }

        // Schedule next cycle of work
        if (loop)
        {
            RunAsync([this]() { DoWork(); }, m_pollingInterval);
        }
        else
        {
            // clear the outgoing queue
            std::lock_guard<std::mutex> lock(m_outgoingLock);
            std::queue<OutgoingQueuedItem> empty;
            std::swap(m_outgoingQueue, empty);
        }
    }

    void CSpxWebSocket::DoConnect()
    {
        bool success = false;
        auto error = WebSocketError::UNKNOWN;
        int errorCode = 0;
        std::string errorMessage;

        std::chrono::steady_clock::duration connectTime(0);
        auto start = std::chrono::steady_clock::now();

        try
        {
            // TODO telemetry: MetricsTransportStart(m_telemetry, m_connectionId);
            // TODO upload rate reset here
            m_webSocket->Open(m_context);

            // TODO how are redirects handled?

            success = true;
        }
        catch (const http::TransportException& ex)
        {
            success = false;
            error = WebSocketError::CONNECTION_FAILURE;
            errorCode = 1;
            errorMessage = m_errorHandler->GenerateSendErrorMessage(
                HttpMethod::Get, m_endpointInfo.get(), ex.what());
        }
        catch (const core::RequestFailedException& ex)
        {
            success = false;
            error = WebSocketError::WEBSOCKET_UPGRADE;
            errorCode = (int)ex.StatusCode;

            ResponseWrapper wrapper(ex.RawResponse.get(), m_endpointInfo.get(), m_errorHandler.get());
            errorMessage = m_errorHandler->GenerateResponseErrorMessage(
                HttpMethod::Get, m_endpointInfo.get(), &wrapper);
        }
        catch (const std::exception& ex)
        {
            success = false;
            error = WebSocketError::WEBSOCKET_ERROR;
            errorCode = -1;
            errorMessage = m_errorHandler->GenerateSendErrorMessage(
                HttpMethod::Get, m_endpointInfo.get(), ex.what());
        }
        catch (...)
        {
            success = false;
            error = WebSocketError::WEBSOCKET_ERROR;
            errorCode = -1;
            errorMessage = "Unknown error while trying to open the web socket connection";
        }

        connectTime = std::chrono::steady_clock::now() - start;
        std::string utcTimeStamp = PAL::GetUtcTimestamp();

        if (success)
        {
            SPX_TRACE_INFO("[0x%p] Opening web socket completed in %" PRId64 " ms. Timestamp: %s",
                (void*)this,
                (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(connectTime).count(),
                utcTimeStamp.c_str());

            // TODO telemetry: MetricsTransportConnected

            // Change the state to indicate we are open. The next time we pump the worker thread, we
            // will start sending messages
            ChangeState(WebSocketState::OPENING, WebSocketState::CONNECTED);

            // Azure core has a blocking function call to read incoming web socket messages that unfortunately
            // does not have a timeout. To work around this, we will start an independent thread to receive
            // messages, and add them to a queue that will be processed in our worker thread.
            // The destructor will always try to join on the receive thread so we can directly pass "this"
            m_receiveThread = std::thread(&CSpxWebSocket::ReceiveThread, this);

            SendConnectedEvent(m_endpointInfo.get()->EndpointUrl());
        }
        else
        {
            SPX_TRACE_ERROR("[0x%p] Opening web socket failed in %" PRId64 " ms. Timestamp: %s, Error: %s, Code: %d, Message: %s",
                (void*)this,
                (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(connectTime).count(),
                utcTimeStamp.c_str(),
                EnumHelpers::ToString(error),
                errorCode,
                errorMessage.c_str());

            // TODO telemetry?
            //MetricsTransportDropped();
            //MetricsTransportError(m_telemetry, m_connectionId, open_result_detailed.code);

            HandleFatalError(error, errorCode, errorMessage);
        }
    }

    void CSpxWebSocket::DoReceive(const size_t max)
    {
        try
        {
            for (size_t i = 0; i < max; i++)
            {
                IncomingQueuedItem received;
                {
                    Lock lock(m_incomingLock);
                    if (m_incomingQueue.empty())
                    {
                        break;
                    }

                    received = std::move(m_incomingQueue.front());
                    m_incomingQueue.pop();
                }

                int64_t timeInQueueMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - received.queuedTime)
                    .count();

                size_t size = received.type == WebSocketFrameType::Binary
                    ? received.binaryData.size()
                    : received.type == WebSocketFrameType::Text
                    ? received.textData.size()
                    : 0;

                SPX_TRACE_INFO("[0x%p] Processing received web socket message. Received: %s, TimeInQueue: %" PRId64 "ms, Type: %s, Size: %zu B",
                    (void*)this, received.utcTimeStamp.c_str(), timeInQueueMs, EnumHelpers::ToString(received.type), size);

                switch (received.type)
                {
                case WebSocketFrameType::Binary:
                    SendBinaryDataEvent(received.binaryData.data(), received.binaryData.size());
                    break;

                case WebSocketFrameType::Text:
                    SendTextDataEvent(received.textData);
                    break;

                case WebSocketFrameType::Close:
                {
                    ChangeState(WebSocketState::CONNECTED, WebSocketState::CLOSED);
                    // TODO telemetry: MetricsTransportDropped();
                    // TODO figure also block new messages from being queued?

                    SendDisconnectedEvent(received.disconnectReason, received.textData, true);

                    // since web socket is now closed, there should be nothing more to receive
                    size_t remainingReceived = 0;
                    {
                        Lock lock(m_incomingLock);
                        remainingReceived = m_incomingQueue.size();
                        std::queue<IncomingQueuedItem> _{};
                        m_incomingQueue.swap(_);
                    }

                    SPX_TRACE_WARNING_IF(
                        remainingReceived > 0,
                        "[0x%p] Web socket close frame was received, followed by other messages from the service. Ignoring remaining messages",
                        (void*)this);
                    return;
                }

                case WebSocketFrameType::Unknown:
                    SPX_TRACE_WARNING("[0x%p] Unknown web socket frame will be ignored", (void*)this);
                    break;

                default:
                    SPX_TRACE_WARNING("[0x%p] Unknown web socket frame will be ignored (%d)", (void*)this, (int)received.type);
                }
            }
        }
        catch (const std::exception& ex)
        {
            HandleFatalError(
                WebSocketError::WEBSOCKET_ERROR,
                (int)InternalWebSocketError::ReceiveWorkerError,
                std::string("Failed while processing received web socket message. Details: ") + ex.what());
        }
        catch (...)
        {
            HandleFatalError(
                WebSocketError::WEBSOCKET_ERROR,
                (int)InternalWebSocketError::ReceiveWorkerError,
                "Failed while processing received web socket message");
        }
    }

    void CSpxWebSocket::DoSend(const size_t max)
    {
        OutgoingQueuedItem outgoing;
        std::string errorMessage;
        InternalWebSocketError errorCode{ InternalWebSocketError::None };

        try
        {
            for (size_t i = 0; i < max; i++)
            {
                {
                    Lock _(m_outgoingLock);
                    if (m_outgoingQueue.empty())
                    {
                        return;
                    }

                    outgoing = std::move(m_outgoingQueue.front());
                    m_outgoingQueue.pop();
                }

                if (outgoing.Message() == nullptr)
                {
                    SPX_TRACE_WARNING("[0x%p] Web socket had a null message queued for sending. Ignoring", (void*)this);
                    continue;
                }

                int64_t timeInQueueMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - outgoing.queuedTime)
                    .count();

                SPX_TRACE_INFO("[0x%p] Web socket sending message. TimeInQueue: %" PRId64 "ms, %s",
                    (void*)this, timeInQueueMs, outgoing.Message()->LogDescription().c_str());

                // TODO telemetry
                //if (static_cast<MetricMessageType>(packet->MetricMessageType()) != MetricMessageType::METRIC_MESSAGE_TYPE_INVALID)
                //{
                //    try
                //    {
                //        MetricsTransportStateStart(packet->MetricMessageType());
                //    }
                //    catch (...) { /* don't care */ }
                //}

                // TODO support chunking?

                switch (outgoing.Message()->FrameType())
                {
                case WebSocketFrameType::Binary:
                {
                    // TODO Optimize the code so we don't create an extra copy here. Could either:
                    // - Modify the message code to use a vector as the underlying storage type
                    // - Ask for the Azure core web socket code to accept a pointer to uint8_t, a size
                    //   and frame type (since it is synchronous code, should be OK anyway)
                    std::vector<uint8_t> serialized(outgoing.Message()->Size());
                    size_t actualSize = outgoing.Message()->Serialize(serialized.data(), serialized.size());
                    serialized.resize(actualSize);

                    m_webSocket->SendFrame(serialized, true, m_context);
                }
                    break;

                case WebSocketFrameType::Text:
                {
                    // TODO optimize code so we don't create an extra copy here. Could either:
                    // - Optimize code to use string as underlying storage type
                    // - Ask for azure core to have a method that accepts a uint8_t pointer, size, and frame type
                    std::string serialized(outgoing.Message()->Size(), '\0');
                    size_t actualSize = outgoing.Message()->Serialize(
                        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(serialized.c_str())),
                        serialized.size());

                    serialized.resize(actualSize, '\0');

                    m_webSocket->SendFrame(serialized, true, m_context);
                }
                    break;

                default:
                case WebSocketFrameType::Close:
                case WebSocketFrameType::Unknown:
                    // Should never get here
                    SPX_TRACE_WARNING("[0x%p] Web socket trying to send unsupported/unknown message. Ignoring", (void*) this);
                    break;
                } // switch

                outgoing.TrySetComplete();

            } // while
        }
        catch (const http::TransportException& ex)
        {
            errorMessage = FAILED_TO_SEND_DETAILS + ex.what();
            errorCode = InternalWebSocketError::SendWorkerTransport;

            ExceptionWithCallStack error(errorMessage, FAILED_TO_SEND_ERR);
            outgoing.TrySetFailed(std::make_exception_ptr(error));
        }
        catch (const std::exception& ex)
        {
            errorMessage = FAILED_TO_SEND_DETAILS + ex.what();
            errorCode = InternalWebSocketError::SendWorkerError;

            ExceptionWithCallStack error(errorMessage, FAILED_TO_SEND_ERR);
            outgoing.TrySetFailed(std::make_exception_ptr(error));
        }
        catch (...)
        {
            errorMessage = FAILED_TO_SEND_MSG;
            errorCode = InternalWebSocketError::SendWorkerError;

            ExceptionWithCallStack error(errorMessage, FAILED_TO_SEND_ERR);
            outgoing.TrySetFailed(std::make_exception_ptr(error));
        }

        // TODO should raise error event here? Old code did not
        if (errorCode != InternalWebSocketError::None)
        {
            HandleFatalError(
                WebSocketError::WEBSOCKET_SEND_FRAME,
                (int)errorCode,
                errorMessage);
        }
    }

    void CSpxWebSocket::ReceiveThread()
    {
        std::vector<uint8_t> buffer;
        buffer.reserve(4096);
        bool closed = false;

        // TODO make this configurable?
        const size_t MAX_MESSAGE_SIZE = 10 * 1024 * 1024; // 10MB
        try
        {
            while (!closed)
            {
                try
                {
                    auto state = m_state.load(std::memory_order_acquire);
                    if (!IsOpen(state))
                    {
                        SPX_TRACE_INFO("[0x%p] Web socket receive thread called when web socket not open (%s). Closing",
                            (void*)this, EnumHelpers::ToString(state));
                        return;
                    }

                    // This will block until there is another message to read. In the case that the web socket is closed
                    // it will throw a TransportException that is platform specific
                    auto frame = m_webSocket->ReceiveFrame(m_context);
                    if (frame == nullptr)
                    {
                        SPX_TRACE_WARNING("[0x%p] Web socket receive thread got a null frame. Ignoring", (void*)this);
                        continue;
                    }

                    IncomingQueuedItem parsed;
                    parsed.type = ToFrameType(frame->FrameType);

                    switch (frame->FrameType)
                    {
                    case ws_internal::WebSocketFrameType::BinaryFrameReceived:
                    {
                        auto binaryFrame = frame->AsBinaryFrame();
                        if (binaryFrame == nullptr)
                        {
                            ThrowRuntimeError("Received binary frame could not be converted to WebSocketBinaryFrame");
                        }
                        else if (binaryFrame->IsFinalFrame && buffer.size() == 0)
                        {
                            parsed.binaryData = std::move(binaryFrame->Data);
                        }
                        else
                        {
                            ApppendToBuffer(buffer, binaryFrame->Data, MAX_MESSAGE_SIZE);
                            if (!binaryFrame->IsFinalFrame)
                            {
                                continue;
                            }

                            parsed.binaryData = std::move(buffer);
                            buffer.clear();
                        }
                        break;
                    }

                    case ws_internal::WebSocketFrameType::TextFrameReceived:
                    {
                        auto textFrame = frame->AsTextFrame();
                        if (textFrame == nullptr)
                        {
                            ThrowRuntimeError("Received text frame could not be converted to WebSocketTextFrame");
                        }
                        else if (textFrame->IsFinalFrame && buffer.size() == 0)
                        {
                            parsed.textData = std::move(textFrame->Text);
                        }
                        else
                        {
                            ApppendToBuffer(buffer, textFrame->Text, MAX_MESSAGE_SIZE);
                            if (!textFrame->IsFinalFrame)
                            {
                                continue;
                            }

                            parsed.textData = std::string(buffer.size(), '\0');
                            memcpy(&parsed.textData[0], buffer.data(), parsed.textData.size());
                            buffer.clear();
                        }

                        break;
                    }

                    case ws_internal::WebSocketFrameType::PeerClosedReceived:
                    {
                        auto closeFrame = frame->AsPeerCloseFrame();
                        parsed.disconnectReason = static_cast<WebSocketDisconnectReason>(closeFrame->RemoteStatusCode);
                        parsed.textData = closeFrame->RemoteCloseReason;
                        closed = true;

                        if (buffer.size() > 0)
                        {
                            SPX_TRACE_ERROR("[0x%p] Web socket close message received when we have a previous incomplete message. Some data may be lost.",
                                (void*)this);
                        }

                        buffer.clear();

                        SPX_TRACE_INFO("[0x%p] Web socket received close message. Time: %s, Status: %s (%d), Message: %s",
                            (void*)this,
                            parsed.utcTimeStamp.c_str(),
                            EnumHelpers::ToString(parsed.disconnectReason),
                            (int)parsed.disconnectReason,
                            parsed.textData.c_str());
                        break;
                    }

                    default:
                        ThrowRuntimeError("Received a message with an unknown frame type: " + std::to_string((int)frame->FrameType));
                        return;
                    } // switch

                    {
                        Lock lock(m_incomingLock);
                        m_incomingQueue.push(std::move(parsed));
                    }
                }
                catch (const http::TransportException& ex)
                {
                    // TODO there isn't currently a good cross platform way to distinguish when the web socket has been
                    // closed normally by the client, or we have some other networking failure based only on the exception
                    // thrown. Instead look at the current state
                    auto state = m_state.load(std::memory_order_acquire);
                    if (state >= WebSocketState::DESTROYING)
                    {
                        SPX_TRACE_INFO("[0x%p] Web socket is closing. Ignoring error: %s", (void*)this, ex.what());
                    }
                    else
                    {
                        HandleFatalError(
                            WebSocketError::WEBSOCKET_ERROR,
                            (int)InternalWebSocketError::ReceiveThreadTransport,
                            std::string("Error in receive thread: ") + ex.what());
                    }

                    return;
                }
                catch (const std::exception& ex)
                {
                    HandleFatalError(
                        WebSocketError::WEBSOCKET_ERROR,
                        (int)InternalWebSocketError::ReceiveThreadError,
                        std::string("Error in receive thread: ") + ex.what());
                    return;
                }
                catch (...)
                {
                    HandleFatalError(
                        WebSocketError::WEBSOCKET_ERROR,
                        (int)InternalWebSocketError::ReceiveThreadError,
                        "Unknown error in receive thread");
                    return;
                }
            } // while
        }
        catch (const std::exception& ex)
        {
            SPX_TRACE_ERROR("Web socket receive thread exited due to an error. Details: %s", ex.what());
        }
        catch (...)
        {
            SPX_TRACE_ERROR("Web socket receive thread exited due to an error");
        }
    }

    void CSpxWebSocket::HandleFatalError(WebSocketError error, int32_t errorCode, const std::string& details)
    {
        // This can be called from different threads to run synchronized on the correct thread service thread
        RunSync([this, error, errorCode, &details]()
        {
            // Change to closed state since the error is considered fatal
            auto prevState = m_state.exchange(WebSocketState::CLOSED, std::memory_order_release);

            SPX_TRACE_ERROR("[0x%p] Web socket fatal error in '%s' state. Error: %s, Code: %d, Details: %s",
                (void*)this, EnumHelpers::ToString(prevState), EnumHelpers::ToString(error), errorCode, details.c_str());

            // TODO any other special logic needed here???

            auto errorInfo = ErrorInfo::FromWebSocket(error, errorCode, details);
            this->SendErrorEvent(errorInfo);
        });
    }

    void CSpxWebSocket::RunAsync(std::function<void()> func, const std::chrono::milliseconds& delay)
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_threadService == nullptr);

        std::weak_ptr<ISpxWebSocket> weakKeepAlive(ISpxWebSocket::shared_from_this());

        std::packaged_task<void()> task([this, weakKeepAlive, func = std::move(func)]()
        {
            auto keepAlive = weakKeepAlive.lock();
            if (keepAlive == nullptr)
            {
                SPX_TRACE_WARNING("Web socket run async on destroyed instance. Ignoring");
                return;
            }

            std::string errorMsg("Error while running web socket code on thread service");

            try
            {
                func();
            }
            catch (const std::exception& ex)
            {
                HandleFatalError(
                    WebSocketError::UNKNOWN,
                    (int)InternalWebSocketError::Unknown,
                    errorMsg + ". Details: " + ex.what());
            }
            catch (...)
            {
                HandleFatalError(WebSocketError::UNKNOWN, (int)InternalWebSocketError::Unknown, errorMsg);
            }
        });

        m_threadService->ExecuteAsync(std::move(task), delay, m_affinity);
    }

    void CSpxWebSocket::RunSync(std::function<void()> func)
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_threadService == nullptr);

        if (m_threadService->IsOnThread(m_affinity))
        {
            func();
        }
        else
        {
            std::packaged_task<void()> task(func);
            m_threadService->ExecuteSync(std::move(task), m_affinity);
        }
    }

}}}}
