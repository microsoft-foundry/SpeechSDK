//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// web_socket.cpp: A web socket C++ implementation using the Azure C shared library
//

#include "stdafx.h"
#include "web_socket.h"
#include "azure_c_shared_utility_http_proxy_io_wrapper.h"
#include "azure_c_shared_utility_error_converter.h"
#include "azure_c_shared_utility_platform_wrapper.h"
#include "azure_c_shared_utility_tlsio_wrapper.h"
#include "azure_c_shared_utility_uws_frame_encoder_wrapper.h"
#include "azure_c_shared_utility_xlogging_wrapper.h"
#include <exception.h>
#include <string_utils.h>
#include <sstream>
#include <error_info.h>
#include <http_utils.h>
#include <interfaces/ispx_http_response.h>
#include <default_http_error_handler.h>
#include <time_utils.h>
#include <http_platform.h>
#include "i_web_socket_adapter.h"
#include "web_socket_message.h"
#include "util/promise_helpers.h"
#include "http_platform_impl.h"

#ifdef SPEECHSDK_USE_OPENSSL
#include "azure_c_shared_utility_httpapi_wrapper.h"
#include "azure_c_shared_utility/shared_util_options.h"
#endif

DEFINE_ENUM_STRINGS(WS_OPEN_RESULT, WS_OPEN_RESULT_VALUES)
DEFINE_ENUM_STRINGS(WS_ERROR, WS_ERROR_VALUES)

using namespace std::chrono_literals;
using namespace Microsoft::CognitiveServices::Speech::USP;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    // The maximum number of send requests to process per cycle in the background web socket thread.
    // This prevents us blocking for a long time if the rate of send requests is too high
    constexpr size_t MAX_SEND_PER_CYCLE = 20;

    /// <summary>
    /// The period over which to measure the upload rate
    /// </summary>
    constexpr std::chrono::milliseconds UPLOAD_RATE_PERIOD = 5000ms;

    WebSocketAdapter::WebSocketMessageType ToAdapterMessageType(WebSocketFrameType type)
    {
        switch (type)
        {
        case WebSocketFrameType::Text:
            return WebSocketAdapter::WebSocketMessageType::Text;
        case WebSocketFrameType::Binary:
            return WebSocketAdapter::WebSocketMessageType::Binary;

        default:
        case WebSocketFrameType::Unknown:
            ThrowLogicError("Unsupported web socket frame type: " + std::to_string((int)type));
            break;
        }
    }

    /// <summary>
    /// How many upload rate measurements are taken before the average is reset
    /// </summary>
    constexpr size_t RESET_AVG_UPLOAD_RATE_EVERY = 20;

    struct BoundMessage
    {
        BoundMessage(OutgoingQueuedItem&& item, std::weak_ptr<CSpxWebSocket> instance)
            : item(std::move(item)), instance(instance)
        {
        }

        OutgoingQueuedItem item;
        std::weak_ptr<CSpxWebSocket> instance;
        std::shared_ptr<uint8_t> buffer;
    };

    /// <summary>
    /// Helper class to make the web socket open result status appear like an ISpxHttpResponse so we can re-use
    /// the ISpxHttpErrorHandler
    /// </summary>
    class WsOpenResultHttpResponseWrapper : public ISpxHttpResponse
    {
    private:
        unsigned int m_statusCode;
        std::string m_reasonPhrase;
        std::map<std::string, std::string> m_headers;
        const IHttpEndpointInfo* m_request;
        const ISpxHttpErrorHandler* m_errorHandler;
        const char* m_body;
        size_t m_bodySize;

    public:
        WsOpenResultHttpResponseWrapper(const WS_OPEN_RESULT_DETAILED& openResult, const IHttpEndpointInfo* request, const ISpxHttpErrorHandler* errorHandler) :
            m_statusCode(0),
            m_reasonPhrase(),
            m_headers(),
            m_request(request),
            m_errorHandler(errorHandler),
            m_body(nullptr),
            m_bodySize(0)
        {
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, request == nullptr);
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, errorHandler == nullptr);

            // As per RFC 2616 the first line of a HTTP response looks like:
            // HTTP-Version SP Status-Code SP Reason-Phrase CRLF
            // we already have the status code in the openResult.code, so we just need to parse out the reason phrase
            m_statusCode = openResult.code;
            size_t numSpace = 0;
            size_t offset = 0;
            for (size_t i = 0; i < openResult.buffSize; i++)
            {
                auto c = openResult.buffer[i];
                if (c == ' ' && ++numSpace == 2)
                {
                    offset = i + 1;
                }
                else if (c == '\r' && offset > 0 && offset <= i && offset < openResult.buffSize)
                {
                    m_reasonPhrase = std::string(reinterpret_cast<const char*>(openResult.buffer + offset), i - offset);
                }
                else if (c == '\n')
                {
                    offset = i + 1;
                    break;
                }
            }

            // Now parse the headers
            if (offset < openResult.buffSize)
            {
                offset += HttpUtils::ParseHttpHeaders(
                    static_cast<const uint8_t*>(openResult.buffer + offset),
                    openResult.buffSize - offset,
                    m_headers);
            }

            // Sometimes we have enough space in our buffer for part of the service response
            if (offset < openResult.buffSize)
            {
                m_bodySize = openResult.buffSize - offset;
                m_body = reinterpret_cast<const char*>(openResult.buffer + offset);
            }
        }

        virtual bool IsSuccess() const override
        {
            return m_errorHandler->IsSuccess(this);
        }

        virtual void EnsureSuccess() const override
        {
            m_errorHandler->HandleResponse(HttpMethod::Get, m_request, this);
        }

        virtual unsigned int GetStatusCode() const override { return m_statusCode; }
        virtual std::string GetReasonPhrase() const override { return m_reasonPhrase; }

        virtual std::string GetHeader(const std::string& name) const override
        {
            auto found = m_headers.find(name);
            return found == m_headers.end()
                ? std::string{}
            : found->second;
        }

        std::string GetHeaders() const
        {
            // Dump the map into a single string with carriage returns.
            std::ostringstream headers;
            for (const auto& pair : m_headers) {
                headers << pair.first << ": " << pair.second << "\r\n";
            }
            return headers.str();
        }

        virtual std::string ReadContentAsString(const size_t maxLength = (std::numeric_limits<size_t>::max)()) const override
        {
            if (m_bodySize == 0 || m_body == nullptr)
            {
                return {};
            }

            return std::string(m_body, (std::min)(m_bodySize, maxLength));
        }
    };

    static uint64_t get_epoch_time()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return ms.count();
    }

    CSpxWebSocket::CSpxWebSocket() :
        m_valid(false),
        m_open(false),
        m_telemetry(),
        m_connectionId(),
        m_ratePeriodEnds(),
        m_bytesSentInPeriod(0.0),
        m_avgUploadRateKBPerSec(0.0),
        m_numUploadRateSamples(0),
        m_threadService(),
        m_affinity(ISpxThreadService::Affinity::Background),
        m_pollingIntervalMs(100ms),
        m_creationTime(get_epoch_time()),
        m_connectionTime(),
        m_webSocketUnderlyingOptions(),
        m_webSocket(),
        m_state(WebSocketState::CLOSED),
        m_queue(),
        m_queue_lock(),
        m_exceptionAcrossCBoundary(),
        m_request()
    {
        SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    }

    CSpxWebSocket::~CSpxWebSocket()
    {
        SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

        // stop background work if it is still running
        m_valid = false;

        // We must call Disconnect whenever the underlying adapter exists, NOT
        // only when m_open is true. m_open is set only after OnWebSocketOpened
        // fires; between Connect() (which schedules WorkLoop) and
        // OnWebSocketOpened, the worker thread is active but m_open is false.
        // Without this guard, the adapter's shared_ptr destruction would race
        // with WorkLoop in that window.
        if (m_webSocket != nullptr)
        {
            Disconnect();
        }

        m_open = false;

        m_threadService.reset();
    }

    void CSpxWebSocket::Init(
        const std::shared_ptr<ISpxThreadService>& threadService,
        const ISpxThreadService::Affinity affinity,
        const std::chrono::milliseconds& pollingIntervalMs,
        const std::shared_ptr<ISpxWebSocketTelemetry>& telemetry,
        const std::shared_ptr<ISpxHttpErrorHandler>& httpErrorHandler)
    {
        m_threadService = threadService;
        m_affinity = affinity;
        m_pollingIntervalMs = pollingIntervalMs;
        m_telemetry = telemetry;
        m_httpErrorHandler = httpErrorHandler ? httpErrorHandler : GetDefaultHttpErrorHandler();
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
            m_pollingIntervalMs = std::chrono::milliseconds(kMaxPollingMs);
            return;
        }

        SPX_DBG_TRACE_VERBOSE("%s: Updating polling interval from %lld ms to %lld ms",
            __FUNCTION__,
            static_cast<long long>(m_pollingIntervalMs.count()),
            static_cast<long long>(pollingIntervalMs.count()));
        m_pollingIntervalMs = pollingIntervalMs;
    }

    std::function<void()> CSpxWebSocket::Init()
    {
        auto platformInstance = HttpPlatformImpl::Instance();
        return platformInstance->Init();
    }

    void CSpxWebSocket::Connect(const IHttpEndpointInfo& endpoint, const std::string& connectionId)
    {
        SPX_TRACE_FUNCTION();

        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_threadService == nullptr);

        if (m_open)
        {
            ThrowLogicError("Web socket is already connected.");
        }

        if (!endpoint.IsValid())
        {
            ThrowInvalidArgumentException("Endpoint is not valid");
        }
        else if (endpoint.Scheme() != UriScheme::WS && endpoint.Scheme() != UriScheme::WSS)
        {
            ThrowInvalidArgumentException("You must specify a WS or WSS scheme for the endpoint");
        }

        if (!m_connectionId.empty() && NO_DASH_UUID_LEN < connectionId.size() + 1)
        {
            ThrowInvalidArgumentException("Invalid size of connection Id. Please use a valid GUID with dashes removed.");
        }

        m_connectionId = connectionId;
        m_request = endpoint.Clone();

        // We need to hold copies of some returned strings and structures until after we call the Azure C shared library
        // functions. Internally copies will be made of any passed in const char*
        const std::string webSocketProtocols = endpoint.WebSocketProtocols();
        const std::string host = endpoint.Host();
        const int port = endpoint.Port();
        const std::string pathAndQuery = endpoint.Path() + endpoint.QueryString();
        const auto proxy = endpoint.Proxy();

        WS_PROTOCOL wsProto;
        wsProto.protocol = webSocketProtocols.c_str();
        int protocolCount = static_cast<int>(endpoint.WebSocketProtocolsSize());

        // create the underlying web socket handle
        m_webSocket = GetWebSocketAdapter();
        WebSocketAdapter::WebSocketConfiguration webSocketConfiguration;
        webSocketConfiguration.host = host;
        webSocketConfiguration.port = port;
        webSocketConfiguration.relative_path = pathAndQuery;
        webSocketConfiguration.use_ssl = endpoint.IsSecure();
        webSocketConfiguration.protocol_name = protocolCount > 0 ? wsProto.protocol : "";
        webSocketConfiguration.protocol_count = protocolCount;
        if (!proxy.host.empty())
        {
            WebSocketAdapter::ProxyConfiguration proxyConfiguration;
            proxyConfiguration.host = proxy.host;
            proxyConfiguration.port = proxy.port;
            proxyConfiguration.username = proxy.username;
            proxyConfiguration.password = proxy.password;
            m_webSocket->Initialize(webSocketConfiguration, proxyConfiguration, static_cast<void*>(this));
        }
        else
        {
            m_webSocket->Initialize(webSocketConfiguration, static_cast<void*>(this));
        }

        if (m_webSocket == nullptr)
        {
            ThrowRuntimeError("Failed to create the web socket");
            return;
        }

        // set the headers
        for (const auto& entry : endpoint.Headers())
        {
            m_webSocket->SetRequestHeader(entry.first.c_str(), entry.second.c_str());
        }

        // save the underlying options to be applied after we connect
        m_webSocketUnderlyingOptions = endpoint.UnderlyingOptions();
#ifdef SPEECHSDK_USE_OPENSSL
        if (endpoint.IsSecure())
        {
            int tls_version = OPTION_TLS_VERSION_1_2;
            if (m_webSocket->SetOption(OPTION_TLS_VERSION, &tls_version) != HTTPAPI_OK)
            {
                ThrowRuntimeError("Could not set TLS 1.2 option");
            }

            const bool disableDefaultVerifyPaths = endpoint.DisableDefaultVerifyPaths();
            const auto singleTrustedCert = endpoint.SingleTrustedCertificate();
            const bool disableCrlChecks = endpoint.DisableCrlChecks();
            const bool continueOnCrlDownloadFailure = endpoint.ContinueOnCrlDownloadFailure();
            const auto maxCrlDownloadSizeInKB = endpoint.MaxCRLDownloadSizeOption();

            m_webSocket->SetOption(OPTION_DISABLE_DEFAULT_VERIFY_PATHS, &disableDefaultVerifyPaths);
            if (!singleTrustedCert.empty())
            {
                m_webSocket->SetOption(OPTION_TRUSTED_CERT, singleTrustedCert.c_str());
            }

            if(disableCrlChecks)
            {
                m_webSocket->SetOption(OPTION_DISABLE_CRL_CHECK, &disableCrlChecks);
            }

            if(continueOnCrlDownloadFailure)
            {
                m_webSocket->SetOption(OPTION_CONTINUE_ON_CRL_DOWNLOAD_FAILURE, &continueOnCrlDownloadFailure);
            }

            if (maxCrlDownloadSizeInKB > 0)
            {
                m_webSocket->SetOption(OPTION_SSL_CRL_MAX_SIZE_IN_KB, &maxCrlDownloadSizeInKB);
            }
        }
#endif

        ChangeState(WebSocketState::CLOSED, WebSocketState::INITIAL);

        m_valid = true;
        WorkLoop(ISpxInterfaceBaseFor<CSpxWebSocket>::shared_from_this());
    }

    // Performs the actual native teardown of the underlying WebSocket
    // (the UwsWebSocket, which in turn owns the uws_client handle).
    // This is the only path in the SDK that destroys the underlying
    // uws_client. It is called only from CSpxWebSocket::Disconnect()
    // while m_workMutex is held.
    //
    // Invariant: m_workMutex serializes PerformTeardown() against
    // DoWork() (which also acquires m_workMutex at its top). Because
    // uws_client_dowork() runs only inside DoWork(), and the
    // underlying C library's uws_client_destroy() runs only here
    // (transitively, via ~UwsWebSocket -> uws_client_destroy), the
    // mutex guarantees uws_client_dowork() is not executing
    // concurrently with uws_client_destroy(). This closes the
    // historical race in azure-c-shared-utility where
    // uws_client_destroy clearing pending_sends could race with
    // on_underlying_io_send_complete.
    //
    // Do NOT add other call paths to PerformTeardown() or to
    // m_webSocket = nullptr without preserving this mutex hold. Do
    // NOT remove the lock_guard in Disconnect() without an
    // equivalent serialization mechanism.
    void CSpxWebSocket::PerformTeardown()
    {
        if (m_webSocket != nullptr)
        {
            if (m_open)
            {
                // Log the upload speed
                if (m_bytesSentInPeriod > 0)
                {
                    auto startOfRatePeriod = m_ratePeriodEnds - UPLOAD_RATE_PERIOD;
                    auto deltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startOfRatePeriod);

                    double uploadRateKBPerSecond = m_bytesSentInPeriod / deltaMs.count() / 1.024; // 1.024 to convert to B/ms to KB/s
                SPX_TRACE_INFO("[%p] Web socket upload rate this period was is %.4lf KB/s", static_cast<void*>(this), uploadRateKBPerSecond);
                }

                // starts the close handshake.
                SPX_TRACE_INFO("%s: [%p] start the close handshake.", __FUNCTION__, (void*)this);
                m_webSocket->Close(m_pollingIntervalMs, [](void* ctxt) { cast_and_invoke(ctxt, &CSpxWebSocket::OnWebSocketClosed); });
                SPX_TRACE_INFO("%s: [%p] isOpen: %s", __FUNCTION__, static_cast<void*>(this), m_open ? "true" : "false");
            }

            SPX_TRACE_INFO("%s: [%p] destroying uwsclient.", __FUNCTION__, static_cast<void*>(this));
            m_webSocket = nullptr;
        }
    }

    void CSpxWebSocket::Disconnect()
    {
        // Serialize Disconnect() against DoWork() and against concurrent
        // Disconnect() callers. m_workMutex must be acquired BEFORE
        // observing m_state, so that a second caller entering while the
        // first is still in teardown will block here on the mutex (rather
        // than observing DESTROYING in an unprotected window and returning
        // prematurely from a wait branch that races the state transition).
        //
        // The native teardown must not race with an in-flight DoWork on
        // any worker thread (including a thread service worker that has
        // already been Term()'d-then-detached and is still wedged inside
        // socket I/O). Acquiring m_workMutex here blocks until any such
        // DoWork() releases it, and prevents any new DoWork() from starting
        // until PerformTeardown() returns and we release the lock.
        //
        // m_workMutex is recursive because HandleError() called from inside
        // DoWork() may reach this function via the error callback chain
        // (HandleError -> OnError -> ResetWebSocket -> ~CSpxWebSocket ->
        // Disconnect), in which case the same thread will re-enter the lock.
        std::lock_guard<std::recursive_mutex> guard(m_workMutex);

        auto state = m_state.load();
        if (state == WebSocketState::DESTROYING || state == WebSocketState::CLOSED)
        {
            // Either teardown has already completed on another thread (we
            // observed DESTROYING only because we waited on m_workMutex
            // until that thread released it), or there is nothing to tear
            // down. Both cases are no-ops.
            return;
        }

        ChangeState(WebSocketState::DESTROYING);

        // Stop further WorkLoop re-postings. The mutex above is the
        // authoritative serialization; this flag is a fast-path early-out
        // for WorkLoop tasks that have not yet acquired the mutex.
        m_valid = false;

        PerformTeardown();

        MetricsTransportClosed();
    }

    void CSpxWebSocket::SendTextData(const std::string & text)
    {
        if (text.empty())
        {
            return;
        }

        QueueMessage(std::make_shared<WebSocketMessage>(text));
    }

    void CSpxWebSocket::SendBinaryData(const uint8_t * data, const size_t size)
    {
        if (data == nullptr)
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

    int CSpxWebSocket::Connect()
    {
        if (m_open)
        {
            return 0;
        }
        else if (nullptr == m_webSocket)
        {
            return -1;
        }
        else
        {
            SPX_TRACE_INFO(R"(Start to open websocket. WebSocket: [%p], wsio handle: %p)", static_cast<void*>(this), static_cast<void*>(m_webSocket.get()));

            MetricsTransportStart(m_telemetry, m_connectionId);

            m_ratePeriodEnds = std::chrono::steady_clock::now();
            m_bytesSentInPeriod = 0.0;
            m_avgUploadRateKBPerSec = 0.0;
            m_numUploadRateSamples = 0;
            const int result = m_webSocket->Open(
                [](void* ctxt, WS_OPEN_RESULT_DETAILED result)
                {
                    cast_and_invoke(ctxt, &CSpxWebSocket::OnWebSocketOpened, result);
                },
                [](void* ctxt, unsigned char frame_type, const unsigned char* buffer, size_t size)
                {
                    cast_and_invoke(ctxt, &CSpxWebSocket::OnWebSocketFrameReceived, frame_type, buffer, size);
                },
                [](void* ctxt, uint16_t* closeCode, const unsigned char* extraData, size_t extraDataLength)
                {
                    cast_and_invoke(ctxt, &CSpxWebSocket::OnWebSocketPeerClosed, closeCode, extraData, extraDataLength);
                },
                [](void* ctxt, WS_ERROR errorCode)
                {
                    cast_and_invoke(ctxt, &CSpxWebSocket::OnWebSocketError, errorCode);
                });

            return result;
        }
    }

    void CSpxWebSocket::QueueMessage(const std::shared_ptr<IWebSocketMessage>& packet)
    {
        if (GetState() == WebSocketState::CLOSED)
        {
            SPX_TRACE_ERROR("Trying to send on a previously closed socket [%p]", static_cast<void*>(this));
            MetricsTransportInvalidStateError();

            throw ExceptionWithCallStack("Web socket is not open", SPXERR_INVALID_STATE);
        }

        OutgoingQueuedItem queued(packet);
        {
            std::lock_guard<std::mutex> lock(m_queue_lock);
            m_queue.push(std::move(queued));
        }
    }

    int CSpxWebSocket::SendMessage(OutgoingQueuedItem&& item)
    {
        if (item.Message() == nullptr)
        {
            SPX_TRACE_WARNING("[%p] Web socket send message called with a null message. Ignoring", static_cast<void*>(this));
            return -1;
        }

        auto timestamp = PAL::GetUtcTimestamp();
        int64_t timeInQueueMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - item.queuedTime)
            .count();

        SPX_TRACE_VERBOSE("[%p] Web socket sending message. Time: %s, TimeInQueue: %" PRIi64 "ms, %s",
            (void*)this, 
            timestamp.c_str(), 
            timeInQueueMs, 
            item.Message()->LogDescription().c_str());

        // reset the queued time so we can use it to measure how long it took to send the message
        item.queuedTime = std::chrono::steady_clock::now();

        // get the serialized buffer of data to send
        auto boundMessage = std::make_unique<BoundMessage>(std::move(item), ISpxInterfaceBaseFor<CSpxWebSocket>::shared_from_this());
        size_t bytes = 0;
        try
        {
            bytes = boundMessage->item.Message()->Serialize(boundMessage->buffer);
        }
        catch (const std::exception& ex)
        {
            SPX_TRACE_ERROR("[%p] Web socket send message failed during serialization. Details: %s", static_cast<void*>(this), ex.what());
            boundMessage->item.Message()->SetMessageSendFailed(std::current_exception());

            return -2;
        }

        auto mySocket = m_webSocket;
        if(nullptr == mySocket)
        {
            SPX_TRACE_ERROR("[%p] Web socket send message failed because the web socket is null", static_cast<void*>(this));
            boundMessage->item.Message()->SetMessageSendFailed(std::make_exception_ptr(ExceptionWithCallStack("Web socket is not open", SPXERR_INVALID_STATE)));
            return SPXERR_INVALID_STATE;
        }

        // TODO: This does not handle breaking up large payloads into multiple chunks
        int err = mySocket->Send(
            boundMessage->buffer.get(),
            bytes,
            ToAdapterMessageType(boundMessage->item.Message()->FrameType()),
            [](void* ctxt, WS_SEND_FRAME_RESULT result)
            {
                std::unique_ptr<BoundMessage> boundMessage(static_cast<BoundMessage*>(ctxt));
                if (boundMessage != nullptr)
                {
                    auto instance = boundMessage->instance.lock();
                    if (instance != nullptr)
                    {
                        instance->HandleWebSocketFrameSent(boundMessage->item, result);
                    }
                }
            },
            boundMessage.get());

        if (err)
        {
            SPX_TRACE_ERROR("[%p] Web socket send message transfer failed with %d", static_cast<void*>(this), err);
        }
        else
        {
            // Release control of the bound message memory so the callback can delete it
            boundMessage.release();
        }

        return err;
    }

    void CSpxWebSocket::HandleConnected(const std::string& url)
    {
        if (m_valid)
        {
            OnConnected(url);
        }
    }

    void CSpxWebSocket::HandleDisconnected(WebSocketDisconnectReason reason, const std::string& cause, bool serverRequested)
    {
        OnDisconnected(reason, cause, serverRequested);
    }

    void CSpxWebSocket::HandleTextData(const std::string& data)
    {
        OnTextData(data);
    }

    void CSpxWebSocket::HandleBinaryData(const uint8_t * data, const size_t size)
    {
        OnBinaryData(data, size);
    }

    void CSpxWebSocket::HandleError(WebSocketError reason, int errorCode, const std::string& errorMessage)
    {
        if (m_valid)
        {
            auto error = ErrorInfo::FromWebSocket(reason, errorCode, errorMessage);
            if (error)
            {
                const std::string& details = error->GetDetails();
                const char* detailText = details.empty() ? "<no-details>" : details.c_str();
                const char* retry = error->GetRetryMode() == ISpxErrorInformation::RetryMode::Allowed ? "Allowed" : "NotAllowed";
                SPX_TRACE_ERROR("WS [0x%p] err reason=%d category=%d status=%d retry=%s details=%s",
                    (void *)this,
                    static_cast<int>(reason),
                    error->GetCategoryCode(),
                    error->GetStatusCode(),
                    retry,
                    detailText);
            }
            OnError(error);
        }
    }

    void CSpxWebSocket::HandleWebSocketStateChanged(WebSocketState oldState, WebSocketState newState)
    {
        OnStateChanged(oldState, newState);
    }

    void CSpxWebSocket::HandleWebSocketFrameSent(OutgoingQueuedItem& item, WS_SEND_FRAME_RESULT result)
    {
        // Capture a strong reference to the message object once, before any call that could
        // trigger concurrent release on another thread.
        auto message = item.Message();
        if (message == nullptr)
        {
            SPX_TRACE_WARNING("[%p] Web socket send message completed with null message. Ignoring", static_cast<void*>(this));
            return;
        }

        int64_t timeTakenMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - item.queuedTime)
            .count();

        std::string info = message->LogDescription();

        SPX_TRACE_VERBOSE(
            "[%p] Web socket send message completed. Result: %d, SendTime: %" PRIi64 "ms, %s",
            (void*)this, (int)result, timeTakenMs, info.c_str());

        if (result == WS_SEND_FRAME_RESULT::WS_SEND_FRAME_OK)
        {
            item.TrySetComplete();
        }
        else
        {
            auto pEx = std::make_exception_ptr(ExceptionWithCallStack("Failed with code: " + std::to_string((int)result), SPXERR_NETWORK_SEND_FAILED));
            item.TrySetFailed(pEx);
        }

        auto msgType = message->MetricMessageType();
        if (static_cast<MetricMessageType>(msgType) != MetricMessageType::METRIC_MESSAGE_TYPE_INVALID)
        {
            MetricsTransportStateEnd(msgType);
        }

        // update the upload rate heuristics
        auto current = std::chrono::steady_clock::now();
        if (current >= m_ratePeriodEnds)
        {
            if (m_bytesSentInPeriod > 0)
            {
                double uploadRateKBPerSecond = m_bytesSentInPeriod / UPLOAD_RATE_PERIOD.count() / 1.024;

                // update running average
                if (m_numUploadRateSamples < RESET_AVG_UPLOAD_RATE_EVERY)
                {
                    // this uses formula to compute a cumulative moving average. More details can be found here: https://www.wikipedia.org/wiki/Moving_average
                    m_avgUploadRateKBPerSec = m_avgUploadRateKBPerSec + ((uploadRateKBPerSecond - m_avgUploadRateKBPerSec) / (1 + m_numUploadRateSamples));
                    m_numUploadRateSamples++;
                }
                else
                {
                    // "reset" the average by taking the previous value a single entry rather than a running average
                    // this should help the average more closely follow more recent values
                    m_avgUploadRateKBPerSec = (m_avgUploadRateKBPerSec + uploadRateKBPerSecond) / 2;
                    m_numUploadRateSamples = 1;
                }

                SPX_TRACE_INFO("[%p] Web socket upload rate this period was %.4lf KB/s. Average %.4lf", static_cast<void*>(this), uploadRateKBPerSecond, m_avgUploadRateKBPerSec);
                OnEstimatedUploadRateKBPerSec(static_cast<float>(m_avgUploadRateKBPerSec));
            }

            m_ratePeriodEnds = current + UPLOAD_RATE_PERIOD;
            m_bytesSentInPeriod = 0.0;
        }
        else
        {
            m_bytesSentInPeriod += message->Size();
        }
    }

    void CSpxWebSocket::WorkLoop(std::weak_ptr<CSpxWebSocket> weakPtr)
    {
        /* NOTE:
           To prevent being invoked on a destroyed instance, this uses a weak pointer to check
           that the web socket instance has not been destroyed. However, if the timing is just
           right you could end up with the locked weak_ptr at line 726 becoming the final
           remaining reference to the web socket instance. An example of this is if an error
           callback causes the main thread (and the expected owner of the web socket instance)
           to exit before the body of the packaged task below completes. This has the subtle
           side effect of moving the destructor of the web socket instance to the
           CSpxThreadService thread instead of where you expect leading to an exception being
           thrown.

           In normal usage this does not seem to happen. However some integration tests (e.g.
           "Port specification" in core tests) will trigger this if you don't add a delay after
           waiting for the cancelled event.
        */

        std::packaged_task<void()> task([weakPtr]() -> void
        {
            auto ptr = weakPtr.lock();
            if (ptr == nullptr || !ptr->m_valid || ptr->GetState() == WebSocketState::CLOSED)
            {
                return;
            }

            try
            {
                try
                {
                    ptr->DoWork();
                }
                catch (const std::exception& e)
                {
                    ptr->HandleError(WebSocketError::UNKNOWN, -1, e.what());
                }
                catch (...)
                {
                    ptr->HandleError(WebSocketError::UNKNOWN, -1, "Unhandled exception in the USP layer.");
                }
            }
            catch (const std::exception& ex)
            {
                (void)ex;
                SPX_TRACE_ERROR("%s [%p] Unexpected Exception %s. Thread terminated", __FUNCTION__, static_cast<void*>(ptr.get()), ex.what());
            }
            catch (...)
            {
                SPX_TRACE_ERROR("%s [%p] Unexpected Exception. Thread terminated", __FUNCTION__, static_cast<void*>(ptr.get()));
            }

            std::packaged_task<void()> task([ptr]() { WorkLoop(ptr); });
            ptr->m_threadService->ExecuteAsync(std::move(task), ptr->m_pollingIntervalMs, ptr->m_affinity);
        });

        auto ptr = weakPtr.lock();
        if (ptr == nullptr || !ptr->m_valid || ptr->GetState() == WebSocketState::CLOSED)
        {
            return;
        }

        ptr->m_threadService->ExecuteAsync(std::move(task), ptr->m_affinity);
    }

    void CSpxWebSocket::DoWork()
    {
        // Serialize against Disconnect()/PerformTeardown(): while we hold this
        // lock, no teardown of m_webSocket / underlying uws_client / xio chain
        // can begin. Once we return and release, teardown is allowed to run.
        // This is the authoritative protection against the use-after-free that
        // arises when a disposing thread destroys the WebSocket while the
        // worker thread is still inside a Send / DoWork cycle.
        //
        // m_workMutex is recursive so that HandleError() called from within
        // this DoWork() can re-enter via the error callback chain
        // (HandleError -> OnError -> ResetWebSocket -> ~CSpxWebSocket ->
        // Disconnect) on the same thread without deadlocking.
        std::lock_guard<std::recursive_mutex> guard(m_workMutex);

        // We first want to read the packets we received from the service. In case of turn.end we make sure
        // we enqueue the telemetry packets inside the request->queue and by the end of this function we will
        // be sure that the packets has reached the azure_c_lib.
        //uws_client_dowork(m_WSHandle);

        // Make a copy of the shared pointer to prevent destruction of the underlying socket while in the DoWork loop.
        auto ourSocket = m_webSocket;
        if (nullptr != ourSocket) // If the socket went null somewhere along the way, it's because Disconnect was called on us.
        {
            ourSocket->DoWork();
        }

        // as a result of doing work, some C++ functions that throw exceptions may have been invoked. Since
        // throwing exceptions into C code is bad, we catch the exceptions in the handlers (see cast_and_invoke)
        // and re-throw the exception here once we have returned from the C code
        if (m_exceptionAcrossCBoundary)
        {
            std::exception_ptr exPtr(m_exceptionAcrossCBoundary);
            m_exceptionAcrossCBoundary = nullptr;
            std::rethrow_exception(exPtr);
        }

        switch (GetState())
        {
            case WebSocketState::CLOSED:
                {
                    std::lock_guard<std::mutex> lock(m_queue_lock);
                    std::queue<OutgoingQueuedItem> empty;
                    std::swap(m_queue, empty);
                }
                break;

            case WebSocketState::DESTROYING:
                // Do nothing. We are waiting for closing the transport request.
                break;

            case WebSocketState::INITIAL:
                SPX_TRACE_INFO("%s: [%p] open transport.", __FUNCTION__, static_cast<void*>(this));
                if (Connect())
                {
                    ChangeState(WebSocketState::INITIAL, WebSocketState::CLOSED);
                    SPX_TRACE_ERROR("[%p] Failed to open transport", static_cast<void*>(this));
                    return;
                }
                else
                {
                    ChangeState(WebSocketState::INITIAL, WebSocketState::OPENING);
                }
                break;

            case WebSocketState::OPENING:
                // waiting for the OnWebSocketOpened callback
                break;

            case WebSocketState::CONNECTED:
                // limit so as not starve receive thread in case of a high rate of packets
                // to send. Rest of queued packets will be handled in the next cycle
                for (size_t numPackets = 0; numPackets < MAX_SEND_PER_CYCLE; numPackets++)
                {
                    OutgoingQueuedItem packet;
                    {
                        std::lock_guard<std::mutex> lock(m_queue_lock);

                        if (m_queue.empty())
                        {
                            break;
                        }
                        else
                        {
                            // Move so we don't increment the reference count since we are removing it from the queue anyway
                            packet = std::move(m_queue.front());
                            m_queue.pop();
                        }
                    }

                    if (packet.Message())
                    {
                        if (static_cast<MetricMessageType>(packet.Message()->MetricMessageType()) != MetricMessageType::METRIC_MESSAGE_TYPE_INVALID)
                        {
                            try
                            {
                                MetricsTransportStateStart(packet.Message()->MetricMessageType());
                            }
                            catch (...) { /* don't care */ }
                        }

                        int res = SendMessage(std::move(packet));
                        if (res != 0)
                        {
                            HandleError(WebSocketError::WEBSOCKET_SEND_FRAME, res, std::string{});
                        }
                    }
                }

                break;
        }
    }

    void CSpxWebSocket::OnWebSocketOpened(WS_OPEN_RESULT_DETAILED open_result_detailed)
    {
        WS_OPEN_RESULT open_result = open_result_detailed.result;

        if (GetState() == WebSocketState::DESTROYING)
        {
            SPX_TRACE_INFO("%s: [%p] request is null or in destroying state, ignore OnWSOpened()", __FUNCTION__, static_cast<void*>(this));
            return;
        }

        std::string utcTimestamp = PAL::GetUtcTimestamp();

        m_open = (open_result == WS_OPEN_OK);
        if (m_open)
        {
            ChangeState(WebSocketState::OPENING, WebSocketState::CONNECTED);
            m_connectionTime = get_epoch_time();

            SPX_TRACE_INFO("Opening websocket completed. TransportRequest: [%p], wsio handle: 0x%p, time: %s",
                (void*)this, 
                (void*)m_webSocket.get(), 
                utcTimestamp.c_str());

            // Set underlying IO options on the connection is connected.
            for (const auto & option : m_webSocketUnderlyingOptions)
            {
                m_webSocket->SetOption(option.first.c_str(), &option.second);
            }

            MetricsTransportConnected(m_telemetry, m_connectionId);

            HandleConnected(m_request->EndpointUrl());
        }
        else
        {
            // It is safe to transition to TRANSPORT_STATE_CLOSED because the
            // connection is not open.  We must be careful with this state for the
            // reasons described in OnTransportError.
            ChangeState(WebSocketState::CLOSED);

            MetricsTransportDropped();
            MetricsTransportError(m_telemetry, m_connectionId, open_result_detailed.code);

            std::string detailedErrorInfo = GetConnectionErrorMessage(open_result, open_result_detailed.code);
            SPX_TRACE_ERROR("WS [0x%p] open operation failed with result=%d(%s), code=%d[0x%08x], time=%s, details=%s",
                (void *)this,
                open_result,
                ENUM_TO_STRING(WS_OPEN_RESULT, open_result),
                open_result_detailed.code,
                open_result_detailed.code,
                utcTimestamp.c_str(),
                detailedErrorInfo.c_str());

            if (open_result == WS_OPEN_ERROR_BAD_RESPONSE_STATUS)
            {
                // interpret the open result as an ISpxHttpResponse
                WsOpenResultHttpResponseWrapper wrapper(open_result_detailed, m_request.get(), m_httpErrorHandler.get());

                HttpStatusCode statusCode = static_cast<HttpStatusCode>(open_result_detailed.code);
                std::string errorString;

                // special case to handle redirects
                if (statusCode == HttpStatusCode::MOVED_PERMANENTLY
                    || statusCode == HttpStatusCode::PERM_REDIRECT
                    || statusCode == HttpStatusCode::TEMP_REDIRECT)
                {
                    errorString = wrapper.GetHeaders();
                }
                else
                {
                    errorString = m_httpErrorHandler->GenerateResponseErrorMessage(HttpMethod::Get, m_request.get(), &wrapper);
                }

                HandleError(WebSocketError::WEBSOCKET_UPGRADE, /* HTTP status */ open_result_detailed.code, errorString);
            }
            else
            {
                std::string detailedError = GetConnectionErrorMessage(open_result, open_result_detailed.code);
                const auto message = m_httpErrorHandler->GenerateSendErrorMessage(
                    HttpMethod::Get,
                    m_request.get(),
                    detailedError);

                HandleError(WebSocketError::CONNECTION_FAILURE, open_result_detailed.result, message.c_str());
            }
        }
    }

    void CSpxWebSocket::OnWebSocketFrameReceived(unsigned char frame_type, const unsigned char * buffer, size_t size)
    {
        if (GetState() == WebSocketState::DESTROYING)
        {
            SPX_TRACE_INFO("%s: [%p] request is in destroying state, ignore OnWSFrameReceived().", __FUNCTION__, static_cast<void*>(this));
            return;
        }

        if (!m_valid || !m_open)
        {
            SPX_TRACE_ERROR("%s: [%p] request is not valid and/or not open", __FUNCTION__, static_cast<void*>(this));
            return;
        }

        std::string parsed;

        switch (frame_type)
        {
            case WS_FRAME_TYPE_TEXT:
                parsed = std::string(reinterpret_cast<const char *>(buffer), size);
                HandleTextData(parsed);
                break;

            case WS_FRAME_TYPE_BINARY:
                HandleBinaryData(static_cast<const uint8_t *>(buffer), size);
                break;

            default:
                SPX_TRACE_ERROR("ProtocolViolation: Unknown message type: %d", frame_type);
                break;
        }
    }

    void CSpxWebSocket::OnWebSocketPeerClosed(uint16_t * closeCode, const unsigned char * extraData, size_t extraDataLength)
    {
        SPX_TRACE_INFO("%s: context=[%p]", __FUNCTION__, static_cast<void*>(this));

        m_open = false;
        ChangeState(WebSocketState::CLOSED);

        MetricsTransportDropped();

        auto reason = static_cast<WebSocketDisconnectReason>(closeCode != nullptr ? *closeCode : -1);
        std::string cause;
        if (extraDataLength > 0)
        {
            cause = std::string(reinterpret_cast<const char *>(extraData), extraDataLength);
        }

        HandleDisconnected(reason, cause, true);
    }

    void CSpxWebSocket::OnWebSocketError(WS_ERROR errorCode)
    {
        SPX_TRACE_ERROR("WS [%p] operation failed with error code=%d(%s)", static_cast<void*>(this), errorCode, ENUM_TO_STRING(WS_ERROR, errorCode));

        m_open = false;
        ChangeState(WebSocketState::CLOSED);

        MetricsTransportDropped();

        HandleError(WebSocketError::WEBSOCKET_ERROR, errorCode, ENUM_TO_STRING(WS_ERROR, errorCode));
    }

    void CSpxWebSocket::OnWebSocketClosed()
    {
        SPX_TRACE_INFO("%s: context=[%p]", __FUNCTION__, static_cast<void*>(this));

        m_open = false;
        ChangeState(WebSocketState::CLOSED);
        MetricsTransportClosed();
        HandleDisconnected(WebSocketDisconnectReason::Normal, "", false);
    }

    uint64_t CSpxWebSocket::GetTimestamp() const
    {
        return get_epoch_time() - m_creationTime;
    }

}}}}
