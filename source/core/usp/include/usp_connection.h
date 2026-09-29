//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <set>
#include <ajv.h>

#include "usp_callbacks.h"
#include "../usp_web_socket.h"
#include "../usp_metrics.h"

#include "interface_helpers.h"
#include "service_helpers.h"
#include "object_with_site_init_impl.h"
#include "ispxinterfaces.h"
#include "interfaces/ispx_usp_connection.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

class ClientConfiguration;

class CSpxUspConnection :
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxGenericSite,
    public ISpxServiceProvider,
    public ISpxUspConnection,
    public ISpxInterfaceBaseFor<CSpxUspConnection>
{

public:

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxUspConnection)
    SPX_INTERFACE_MAP_END()

    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

    CSpxUspConnection();

    // ISpxObjectWithSiteInitImpl
    void Init() override;

    // ISpxUspConnection
    /**
    * Creates a new Impl instance.
    * @param config Specifies USP client configuration parameters (including an instance of the Callbacks class).
    */
    void SetConfiguration(const ClientConfiguration& config) override;

    /**
    * Establishes a new connection to the service, launches a worker thread
    * to process incoming/outgoing requests.
    */
    void Connect() override;

    /**
    * Adds an audio segment to the outgoing queue.
    * @param audioChunk the audio chunk to be queued.
    */
    void QueueAudioSegment(const DataChunkPtr& audioChunk) override;

    /**
    * Adds an empty audio segment to the outgoing queue, which serves as a signal to the service that the audio
    * stream has ended (i.e., has been uploaded completely).
    * @param uspHandle the UspHandle for sending the audio.
    */
    void QueueAudioEnd() override;

    /**
    * Adds a message to the outgoing queue.
    * @param message The message being sent.
    */
    void QueueMessage(std::unique_ptr<Message> message) override;

    /**
    * Writes latency value into telemetry data.
    * @param latencyInTicks The latency value in ticks.
    * @param isPhraseLatency If it is true, the latency is for phrase result. Otherwise it is for hypothesis result.
    */
    void WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency) override;

    /**
     * Flushes telemetry data.
     */
    void FlushTelemetry() override;

    /**
    * Requests the connection to service to be shut down.
    * @param uspContext A pointer to the UspContext.
    */
    void Shutdown() override;

    /**
    * Returns true if the status is connected.
    */
    bool IsConnected() override;

    /**
    * Returns the URL used for connection.
    */
    std::string GetConnectionUrl() override;

    /**
    * Closes the USP connection.
    */
    ~CSpxUspConnection();

private:
    void Invoke(std::function<void(CallbacksPtr)> callback);

    std::string ConstructConnectionUrl() const;

    std::string CreateRequestId();
    void RegisterRequestId(const std::string& requestId);
    std::string UpdateRequestId(const MessageType messageType, bool isBinary);

    std::set<std::string> m_activeRequestIds;
    std::string m_speechRequestId;

    // Inline commit: an audio.commit arriving before a turn exists is held
    // here rather than dropped, and sent once a request id has been
    // established. A commit issued immediately after recognition starts can
    // reach this class a millisecond or so before the speech.context message
    // that creates the id, and dropping it there would make an ordinary call
    // sequence fail depending on thread timing.
    //
    // One slot is enough: commits are rate limited to one per 100 ms per
    // stream and the window is very short, so a second arrival means the
    // first is stale. Guarded by nothing - this class is driven from a single
    // thread through the thread service, like every other member here.
    std::unique_ptr<Message> m_deferredCommit;

    // Temp while collapsing classes.
    std::shared_ptr<ClientConfiguration> m_config;
    bool m_valid;
    bool m_connected;
    bool m_speechContextMessageAllowed;
    std::string m_connectionUrl;
    size_t m_audioOffset;
    std::shared_ptr<ISpxWebSocketTelemetry> m_telemetry;
    std::shared_ptr<UspWebSocket> m_transport;
    const uint64_t m_creationTime;
    bool m_turnUsingHeaders;

    void OnTelemetryData(std::string&& data, const std::string& requestId);
    void OnTransportOpened(const std::string& url);
    void OnTransportClosed(WebSocketDisconnectReason reason, const std::string& details, bool serverRequested);
    void OnTransportError(const std::shared_ptr<ISpxErrorInformation>& error);
    void OnTransportData(bool isBinary, const UspHeaders& headers, const unsigned char* buffer, size_t bufferSize);
    void OnTransportEstimatedUploadRate(const float uploadRateKBPerSecond);

    void OnTransportTextData(const UspHeaders& headers, const std::string& data)
    {
        OnTransportData(false, headers, reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
    }

    void OnTransportBinaryData(const UspHeaders& headers, const uint8_t* data, size_t size)
    {
        OnTransportData(true, headers, reinterpret_cast<const unsigned char*>(data), size);
    }

    void InvokeRecognitionErrorCallback(RecognitionStatus status, const std::string& response);

    uint64_t getTimestamp();
    SpeechPhraseMsg RetrieveSpeechPhraseResult(const ajv::JsonReader& json);
    bool isErrorRecognitionStatus(RecognitionStatus status);
    std::shared_ptr<Microsoft::CognitiveServices::Speech::Impl::ISpxThreadService> m_threadService;

    void FillLanguageForAudioOutputChunkMsg(const std::string& streamId, const std::string& messagePath, AudioOutputChunkMsg& msg);
    std::map<std::string, std::string> m_streamIdLangMap;

    void EnsureTelemetry();
};

}}}}
