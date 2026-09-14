//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// usb_web_socket.cpp: The implementation of the USP web socket connection.
//

#include "stdafx.h"
#include <sstream>
#include <http_utils.h>
#include <time_utils.h>
#include <i_telemetry.h>
#include <error_info.h>
#include <create_object_helpers.h>
#include <interfaces/i_web_socket_init.h>
#include "usp_binary_message.h"
#include "usp_text_message.h"
#include "usp_web_socket.h"
#include "usp_constants.h"
#include "uspcommon.h"
#include "site_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

    using namespace std::chrono_literals;
    using namespace Microsoft::CognitiveServices::Speech::Impl;

    constexpr int WS_MESSAGE_HEADER_SIZE = 2;

    SPX_EXTERN_C void* GetModuleObject(const char* className, uint64_t interfaceTypeId);

    UspWebSocket::UspWebSocket(std::shared_ptr<ISpxWebSocket> webSocket) :
        m_chunkSent(false),
        m_streamId(0),
        m_webSocket(webSocket)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, webSocket == nullptr);
    }

    std::shared_ptr<UspWebSocket> UspWebSocket::Create(
        ISpxThreadService::Ptr threadService,
        ISpxThreadService::Affinity affinity,
        const std::chrono::milliseconds& pollingIntervalMs,
        ISpxWebSocketTelemetry::Ptr telemetry,
        ISpxGenericSite::Ptr site)
    {
        std::shared_ptr<ISpxWebSocket> webSocket = SpxCreateObjectWithSite<ISpxWebSocket>("CSpxQueuingWebSocket", site);
        SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, webSocket == nullptr);

        // init the web socket
        auto webSocketInit = SpxQueryInterface<ISpxWebSocketInit>(webSocket);
        if (webSocketInit != nullptr)
        {
            webSocketInit->Init(threadService, affinity, pollingIntervalMs, telemetry);
        }

        std::shared_ptr<UspWebSocket> instance{ new UspWebSocket(webSocket) };

        // Connect web socket event handlers using the safe
        // Event<>::Add(shared_ptr, &method) overload.
        webSocket->OnBinaryData.Add(instance, &UspWebSocket::HandleBinaryData);
        webSocket->OnConnected.Add(instance, &UspWebSocket::HandleConnected);
        webSocket->OnDisconnected.Add(instance, &UspWebSocket::HandleDisconnected);
        webSocket->OnError.Add(instance, &UspWebSocket::HandleError);
        webSocket->OnTextData.Add(instance, &UspWebSocket::HandleTextData);
        webSocket->OnEstimatedUploadRateKBPerSec.Add(instance, &UspWebSocket::HandleEstimatedUploadRateComputed);

        return instance;
    }

    void UspWebSocket::Connect(const Impl::IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId)
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_webSocket == nullptr);
        SPX_THROW_HR_IF(SPXERR_INVALID_STATE, m_webSocket->IsConnected());

        m_webSocket->Connect(webSocketEndpoint, connectionId);
    }

    void UspWebSocket::Disconnect()
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_webSocket == nullptr);
        m_webSocket->Disconnect();
    }

    void UspWebSocket::SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs)
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_webSocket == nullptr);

        auto webSocketInit = Impl::SpxQueryInterface<Impl::ISpxWebSocketInit>(m_webSocket);
        if (webSocketInit != nullptr)
        {
            webSocketInit->SetPollingInterval(pollingIntervalMs);
        }
    }

    void UspWebSocket::SendAudioData(const std::string& path, const Impl::DataChunkPtr& audioChunk, const std::string& requestId, bool newStream)
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_webSocket == nullptr);

        if (requestId.empty())
        {
            Impl::ThrowInvalidArgumentException("requestId is empty.");
        }

        if (newStream)
        {
            m_streamId++;
        }

        size_t bufferSize = (size_t)audioChunk->size;

        auto message = std::make_shared<USP::BinaryMessage>(
            audioChunk->data, bufferSize, path, USP::MessageType::Audio, requestId);

        std::string contentType = audioChunk->contentType;
        if (audioChunk->isWavHeader && contentType.empty())
        {
            contentType = Constants::CONTENT_TYPE_WAVE;
        }

        // set the rest of the required USP header values
        message->
            Path("audio") // TODO: Why is this hard coded?
            .SetHeader(Constants::HEADER_STREAM_ID, std::to_string(m_streamId.load()))
            .SetHeader(Constants::HEADER_CONTENT_TYPE, contentType)
            .SetHeader(Constants::HEADER_PRESENTATION_TIMESTAMP, audioChunk->capturedTime)
            .SetHeader(Constants::HEADER_SPEAKER_ID, audioChunk->userId);

        auto metricType = MetricMessageType::METRIC_MESSAGE_TYPE_INVALID;
        if (bufferSize == 0)
        {
            metricType = MetricMessageType::METRIC_MESSAGE_TYPE_AUDIO_LAST;

            bool previousWasChunkSent = m_chunkSent.exchange(false);
            if (!previousWasChunkSent)
            {
                return;
            }
        }
        else
        {
            bool previousWasChunkSent = m_chunkSent.exchange(true);
            if (!previousWasChunkSent)
            {
                if (bufferSize < 6)
                {
                    Impl::ThrowInvalidArgumentException("Bad payload");
                }

                if (memcmp(message->Data(), "RIFF", 4) && memcmp(message->Data(), "#!SILK", 6) && memcmp(message->Data(), "OggS", 4))
                {
                    // TODO: Should this throw an exception?
                    return;
                }

                metricType = MetricMessageType::METRIC_MESSAGE_TYPE_AUDIO_START;
            }
        }

        message->MetricMessageType(metricType);

        m_webSocket->SendData(message);
    }

    void UspWebSocket::SendTelemetryData(std::string&& data, const std::string& requestId)
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_webSocket == nullptr);

        m_webSocket->SendData(
            std::make_shared<USP::TextMessage>(
                data, "telemetry", Constants::CONTENT_TYPE_JSON, USP::MessageType::Unknown, requestId));
    }

    void UspWebSocket::SendData(const std::shared_ptr<USP::Message>& message)
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_webSocket == nullptr);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, message == nullptr);
        m_webSocket->SendData(message);
    }

    void UspWebSocket::HandleConnected(const std::string& url)
    {
        OnConnected(url);
    }

    void UspWebSocket::HandleDisconnected(WebSocketDisconnectReason reason, const std::string & cause, bool serverRequested)
    {
        if (reason == WebSocketDisconnectReason::Normal)
        {
            OnDisconnected(reason, cause, serverRequested);
        }
        else
        {
            auto error = ErrorInfo::FromWebSocket(WebSocketError::REMOTE_CLOSED, (int)reason, cause);
            HandleError(error);
        }
    }

    void UspWebSocket::HandleError(const std::shared_ptr<Impl::ISpxErrorInformation>& error)
    {
        OnError(error);
    }

    static void LogReceivedMessage(bool isBinary, const UspHeaders& headers, size_t totalSize)
    {
        try
        {
            std::string path("<!!NO_PATH_SET!!>");
            auto found = headers.find(Constants::HEADER_PATH);
            if (found != headers.end())
            {
                path = found->second;
            }

            SPX_TRACE_VERBOSE("USP message received. IsBinary=%d, Path=%s, Size=%zu B, Time=%s",
                isBinary,
                path.c_str(),
                totalSize,
                PAL::GetUtcTimestamp().c_str());
        }
        catch (...)
        {
            SPX_TRACE_ERROR("USP message received. Unhandled error while logging");
        }
    }

    void UspWebSocket::HandleTextData(const std::string& data)
    {
        UspHeaders headers;
        size_t offset = HttpUtils::ParseHttpHeaders(reinterpret_cast<const uint8_t*>(data.c_str()), data.length(), headers);

        if (offset == 0 || headers.size() == 0)
        {
            PROTOCOL_VIOLATION("Unable to parse response headers%s", "");
            MetricsTransportParsingError();
            return;
        }

        std::string body(data, offset, data.length() - offset);

        LogReceivedMessage(false, headers, data.size());
        OnUspTextData(headers, body);
    }

    void UspWebSocket::HandleBinaryData(const uint8_t* data, const size_t size)
    {
        if (size < WS_MESSAGE_HEADER_SIZE)
        {
            PROTOCOL_VIOLATION("unable to read binary message length%s", "");
            return;
        }

        UspHeaders headers;
        uint16_t headerSize = 0;
        headerSize = (uint16_t)(data[0] << 8);
        headerSize |= (uint16_t)(data[1] << 0);

        size_t offset = HttpUtils::ParseHttpHeaders(data + WS_MESSAGE_HEADER_SIZE, headerSize, headers);
        if (offset > 0)
        {
            offset += WS_MESSAGE_HEADER_SIZE;
        }

        if (offset == 0 || headers.size() == 0)
        {
            PROTOCOL_VIOLATION("Unable to parse response headers%s", "");
            MetricsTransportParsingError();
            return;
        }

        LogReceivedMessage(true, headers, size);
        OnUspBinaryData(headers, data + offset, size - offset);
    }

    void UspWebSocket::HandleEstimatedUploadRateComputed(const float uploadRate)
    {
        OnEstimatedUploadRateKBPerSec(uploadRate);
    }

} } } }
