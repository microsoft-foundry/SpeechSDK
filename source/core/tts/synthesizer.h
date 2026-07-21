//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// synthesizer.h: Implementation declarations for CSpxSynthesizer C++ class
//

#pragma once
#include <queue>
#include <map>
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "property_bag_impl.h"
#include <object_with_site_init_impl.h>
#include "synthesis_helper.h"
#include "synthesizer_timeout_management.h"
#include "synthesis_latency.h"
#include "task_helpers.h"
#include "interfaces/ispx_reco_engine_adapter_token_provider.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

 namespace word_boundary_property
 {
    constexpr auto wordBoundaryItems = "wordBoundaries";
    constexpr auto audioOffset = "audioOffset";
    constexpr auto duration = "duration";
    constexpr auto textOffset = "textOffset";
    constexpr auto wordLength = "wordLength";
    constexpr auto text = "text";
    constexpr auto boundaryType = "boundaryType";
}

class CSpxSynthesizer :
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxSynthesizer,
    public ISpxSynthesizerEvents,
    public ISpxTtsEngineAdapterSite,
    public ISpxServiceProvider,
    public ISpxGenericSite,
    public ISpxPropertyBagImpl,
    public ISpxConnectionFromRecognizer,
    public ISpxRecoEngineAdapterTokenProvider
{
public:
    CSpxSynthesizer();
    virtual ~CSpxSynthesizer();

    SPX_INTERFACE_MAP_BEGIN()
    SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
    SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
    SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
    SPX_INTERFACE_MAP_ENTRY(ISpxSynthesizer)
    SPX_INTERFACE_MAP_ENTRY(ISpxSynthesizerEvents)
    SPX_INTERFACE_MAP_ENTRY(ISpxTtsEngineAdapterSite)
    SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
    SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_ENTRY(ISpxConnectionFromRecognizer)
    SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapterTokenProvider)
    SPX_INTERFACE_MAP_END()

    // --- ISpxObjectInit ---

    void Init() override;
    void Term() override;

    // --- ISpxSynthesizer ---

    void SetOutput(std::shared_ptr<ISpxAudioOutput> output) override;
    std::shared_ptr<ISpxSynthesisResult> Speak(const std::string &text, bool isSsml,
                                               const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) override;
    CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>> SpeakAsync(const std::string &text, bool isSsml,
                                                                 const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) override;
    std::shared_ptr<ISpxSynthesisResult> StartSpeaking(const std::string &text, bool isSsml,
                                                       const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) override;
    CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>> StartSpeakingAsync(const std::string &text, bool isSsml,
                                                                         const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) override;
    void StopSpeaking() override;
    CSpxAsyncOp<void> StopSpeakingAsync() override;

    CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>> GetVoicesAsync(const std::string &locale) override;

    std::shared_ptr<ISpxMessageParamFromUser> GetMessageParamFromUser() override;

    void Close() override;
    void SetDisposing() override;

    std::shared_ptr<std::string> FormatWordBoundaryForCache() override;

    void OpenConnection() override;
    void CloseConnection() override;

    // --- ISpxSynthesizerEvents ---

    void FireResultEvent(std::shared_ptr<ISpxSynthesisResult> result, std::shared_ptr<CountDownLatch> eventsSyncLatch) override;
    void FireWordBoundary(std::string resultId, uint64_t audioOffset, uint64_t duration, uint32_t textOffset,
                          uint32_t wordLength, std::string text,
                          SpeechSynthesisBoundaryType boundaryType) override;
    void FireVisemeReceived(std::string resultId, uint64_t audioOffset, uint32_t visemeId, std::string animation) override;
    void FireBookmarkReached(std::string resultId, uint64_t audioOffset, std::string text) override;
    void FireConnectionChanged(bool connected) override;
    void FireTokenRequest() override;

    // --- ISpxServiceProvider ---

    SPX_SERVICE_MAP_BEGIN()
    SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
    SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_ENTRY_OBJECT(ISpxThreadService, m_threadService)
    SPX_SERVICE_MAP_ENTRY(ISpxRecoEngineAdapterTokenProvider)
    SPX_SERVICE_MAP_END()

    // --- ISpxTtsEngineAdapterSite ---

    uint32_t Write(ISpxTtsEngineAdapter *adapter, const std::string &requestId, uint8_t *buffer, uint32_t size,
                   std::shared_ptr<std::map<std::string, std::string>> properties) override;
    std::shared_ptr<ISpxSynthesizerEvents> GetEventsSite() override;
    std::shared_ptr<ISpxSynthesisResult> CreateEmptySynthesisResult() override;
    std::shared_ptr<ISpxSynthesisVoicesResult> CreateEmptySynthesisVoicesResult() override;
    std::shared_ptr<ISpxVoiceInfo> CreateEmptyVoiceInfo() override;
    void SetAdapterFormat(const ISpxTtsEngineAdapter *adapter, const std::shared_ptr<SPXWAVEFORMATEX> &format) override;
    void FireAdapterResult_WordBoundary(ISpxTtsEngineAdapter *adapter, uint64_t audioOffset, uint64_t duration, uint32_t textOffset, uint32_t wordLength, const std::string &text, SpeechSynthesisBoundaryType boundaryType) override;
    void FireAdapterResult_VisemeReceived(ISpxTtsEngineAdapter *adapter, uint64_t audioOffset, uint32_t visemeId, std::string animation) override;
    void FireAdapterResult_BookmarkReached(ISpxTtsEngineAdapter *adapter, uint64_t audioOffset, const std::string &text) override;
    void FireAdapterResult_ConnectionChanged(ISpxTtsEngineAdapter *adapter, bool connected) override;
    void FireAdapterResult_TurnStarted(ISpxTtsEngineAdapter* adapter) override;
    void EndOfTurn(ISpxTtsEngineAdapter* adapter) override;
    void SetSynthesisResultAudioDuration(ISpxTtsEngineAdapter *adapter, uint64_t duration) override;
    size_t AudioLengthOfCurrentTurn() override;
    std::shared_ptr<ISpxTtsEngineAdapter> GetTtsEngineAdapter() override;
    bool IsStopping() override { return m_shouldStop; }

    // --- ISpxConnectionFromRecognizer ---
    std::shared_ptr<ISpxConnection> GetConnection() override;

    // --- ISpxRecoEngineAdapterTokenProvider
    void RefreshToken() final;

    void EnsureValidToken() override final;

    // --- NamedProperties_Base
    void SetStringValue(const char* name, const char* value) override final;
    void SetBackendName(const std::string& backend) override;
    std::string GetBackendName();

protected:
    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override;

    void CheckLogFilename();

private:
    uint32_t WriteToOutput(const uint8_t *buffer, uint32_t size, const std::string &requestId);

    void PushRequestIntoQueue(const std::string requestId);

    /// <summary>
    /// Wait the current request id at the front of the queue, i.e. current request should start to be processed.
    /// </summary>
    /// <returns>The true if current request is at front, false if it is removed from the queue.</returns>
    bool WaitUntilRequestInFrontOfQueue(const std::string &requestId);
    template <class Rep, class Period>
    bool WaitUntilRequestInFrontOfQueue(const std::string& requestId, const std::chrono::duration<Rep, Period>& timeout);
    void PopRequestFromQueue(const std::string &requestId = "");
    void ClearRequestQueueAndKeepFront();

    /// <summary>
    /// Get the front request id in the queue.
    /// </summary>
    /// <returns>The front request id in the queue.</returns>
    std::string GetFrontRequestId() const;

    std::shared_ptr<ISpxSynthesisResult> CreateResult(const std::string &requestId, ResultReason reason,
                                                      const uint8_t *audio_buffer, size_t audio_length,
                                                      const std::shared_ptr<std::map<std::string, std::string>> &
                                                          properties = nullptr);
    std::shared_ptr<ISpxSynthesisResult> CreateUserCancelledResult(const std::string &requestId);

    enum EventType
    {
        SynthesisResultEvent,
        WordBoundaryEvent,
        VisemeEvent,
        BookmarkEvent,
        ConnectionChanged,
        SynthesisMetadataEndEvent,
        TokenRequestEvent,
    };

    void FireEvent(
        EventType eventType,
        std::shared_ptr<ISpxSynthesisResult> result = nullptr,
        bool connected = true,
        uint64_t offset = 0,
        uint64_t duration = 0,
        uint32_t textOffset = 0,
        uint32_t textLength = 0,
        std::string text = std::string{},
        SpeechSynthesisBoundaryType boundaryType = SpeechSynthesisBoundaryType::Word);
    void DispatchEvent(
        const std::weak_ptr<ISpxSynthesizer> &weakSynthesizer,
        EventType eventType,
        std::string resultId,
        std::shared_ptr<ISpxSynthesisResult> result,
        bool connected,
        uint64_t offset,
        uint64_t duration,
        uint32_t textOffset,
        uint32_t textLength,
        std::string text,
        SpeechSynthesisBoundaryType boundaryType,
        std::shared_ptr<CountDownLatch> eventsSyncLatch = nullptr);
    std::packaged_task<void()> CreateTask(std::function<void()> func);

    void EnsureTtsEngineAdapter();
    void InitializeTtsEngineAdapter();

    std::shared_ptr<ISpxSynthesisResult> ExecuteSynthesis(const std::string &requestId, const std::string &text, bool isSsml,
                                                          const std::shared_ptr<ISpxSynthesisRequest> &request);
    std::shared_ptr<ISpxSynthesisVoicesResult> GetVoices(const std::string &locale);

    std::pair<std::shared_ptr<ISpxSynthesisResult>, std::string> CreateResultFromCache(const std::string &requestId, const std::string &text, bool isSsml);
    std::string CacheResult(const std::string& text, std::shared_ptr<ISpxSynthesisResult> result);

    void InitializeTelemetryManager();

    void WaitForCurrentEventTriggered(uint64_t audioOffset) const;

    void FireWordBoundaryToInternalQueue(uint64_t audioOffset, uint64_t duration, uint32_t textOffset,
        uint32_t wordLength, std::string text,
        SpeechSynthesisBoundaryType boundaryType, const std::string& sourceBackend);
    bool ParseWordBoundaryFromCache(const std::shared_ptr<std::string>& wordBoundaryCache);
    bool FireWordBoundaryFromCache(const std::shared_ptr<std::string>& wordBoundaryCache);

    void LogSynthesisEvent(std::shared_ptr<ISpxSynthesisResult> result, const std::string& requestHash);

private:
    CSpxSynthesizer(const CSpxSynthesizer &) = delete;
    CSpxSynthesizer(const CSpxSynthesizer &&) = delete;

    CSpxSynthesizer &operator=(const CSpxSynthesizer &) = delete;

    std::shared_ptr<ISpxTtsEngineAdapter> m_ttsAdapter;
    std::string m_backendName;
    std::shared_ptr<ISpxAudioOutput> m_audioOutput;
    std::shared_ptr<ISpxAudioRender> m_audioRender;
    std::shared_ptr<ISpxAudioOutput> m_codecBuffer;
    std::shared_ptr<ISpxAudioOutput> m_audioOfCurrentTurn;
    std::shared_ptr<ISpxAudioDataStream> m_audioDataStream;
    std::shared_ptr<ISpxAudioOutput> m_audioDataStreamWriter;
    std::shared_ptr<ISpxAudioStreamReader> m_codecAdapter;
    std::shared_ptr<SynthesisAudioFormat> m_format;
    SpxWAVEFORMATEX_Type m_adapterFormat;
    bool m_needDecoding = false;
    bool m_isUsp = false;
    uint64_t m_synthesisResultAudioDuration = 0;

    int64_t m_synthesisStartedTime{-1};
    int64_t m_audioOutputStartTime{-1};
    int64_t m_decodingStartedTime{-1};
    int64_t m_underrunTime{-1};

    std::shared_ptr<ISpxGenericSite> m_siteKeepAlive;

    std::deque<std::string> m_requestQueue;
    mutable std::mutex m_queueOperationMutex;
    std::mutex m_requestWaitingMutex;
    std::condition_variable m_cv;

    std::mutex m_synthesisStartedMutex;
    std::mutex m_synthesizingMutex;
    std::mutex m_synthesisCompletedMutex;
    std::mutex m_synthesisCanceledMutex;

    std::atomic<bool> m_shouldStop{false};
    std::mutex m_stopMutex;
    std::atomic<bool> m_decodingDone{true};
    std::atomic<bool> m_audioReceived{false};
    std::atomic<bool> m_isDisposing{false};
    bool m_writeToOutputFailed{false};
    std::string m_writeToOutputFailedDetails;

    std::shared_ptr<std::map<std::string, std::string>> m_resultProperties;

    std::shared_ptr<ISpxFileCache> m_cache;
    int m_cacheExpiredDays{};

    std::shared_ptr<ISpxThreadService> m_threadService;
    std::shared_ptr<ISpxThreadService> m_syncCallbacksThreadService;
    std::shared_ptr<SynthesisTimeoutManagement> m_timeoutManagement;
    std::shared_ptr<SynthesisLatency> m_latencies;

    bool m_enableTelemetry{ false };
    std::shared_ptr<ISpxTelemetryManager> m_telemetryManager;

    bool m_eventsSyncToAudio{ true };
    // to ensure synthesis completed event is triggered after all metadata events are triggered
    std::shared_ptr<CountDownLatch> m_eventsSyncLatch;

    void ScheduleTokenRefresh();
    std::packaged_task<void()> GetTokenRefreshTask();

    typedef struct _WordBoundaryData
    {
        std::string sourceBackend;
        uint64_t audioOffset{0};
        uint64_t duration{0};
        uint32_t textOffset{0};
        uint32_t wordLength{0};
        std::string text{""};
        SpeechSynthesisBoundaryType boundaryType{ SpeechSynthesisBoundaryType::Word };
    } WordBoundaryData;
    std::mutex m_wordBoundaryQueueMutex;
    std::deque<WordBoundaryData> m_wordBoundaryQueue;
    std::map<std::string, std::string> m_backendNameByRequestId;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
