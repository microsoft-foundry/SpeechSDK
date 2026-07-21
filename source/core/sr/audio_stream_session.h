//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_stream_session.h: Implementation declarations for CSpxAudioStreamSession C++ class
//

#pragma once

#include <queue>
#include <shared_mutex>
#include <map>

#include "audio_buffer.h"
#include "audio_stream_session_throttle_logic.h"
#include "compressed_audio_adapter.h"
#include "interaction_id_provider.h"
#include "interface_helpers.h"
#include "interfaces/ispx_reco_engine_adapter_token_provider.h"
#include "ispx_telemetry_store_impl.h"
#include "object_with_site_init_impl.h"
#include "packaged_task_helpers.h"
#include "property_bag_impl.h"
#include "self_correcting_sleeper.h"
#include "service_helpers.h"
#include "spxcore_common.h"
#include "thread_service.h"
#include "save_to_wav.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxAudioStreamSession :
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxAudioStreamSessionInit,
    public ISpxAudioProcessor,
    public ISpxServiceProvider,
    public ISpxSession,
    public ISpxGenericSite,
    public ISpxRecognizerSite,
    public ISpxDetectorEngineAdapterSite,
    public ISpxSpeechAudioProcessorAdapterSite,
    public ISpxAudioPumpSite,
    public ISpxRecoEngineAdapterSite,
    public ISpxRecoResultFactory,
    public ISpxEventArgsFactory,
    public ISpxPropertyBagImpl,
    public ISpxSpeechEventPayloadProvider,
    public ISpxGetUspMessageParamsFromUser,
    public ISpxGetUspMessageParamsFromRecognizer,
    public CSpxInteractionIdProvider<CSpxAudioStreamSession>,
    public ISpxAudioReplayer,
    public ISpxTelemetryStoreImpl,
    public ISpxRecoEngineAdapterTokenProvider

{
private:

    using NamedProperties_Base = ISpxPropertyBagImpl;

public:

    CSpxAudioStreamSession();
    virtual ~CSpxAudioStreamSession();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxInterfaceBase)
        SPX_INTERFACE_MAP_ENTRY(ISpxSession)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecognizerSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxDetectorEngineAdapterSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechAudioProcessorAdapterSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioPumpSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapterSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoResultFactory)
        SPX_INTERFACE_MAP_ENTRY(ISpxEventArgsFactory)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamSessionInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessor)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxInteractionIdProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechEventPayloadProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxGetUspMessageParamsFromUser)
        SPX_INTERFACE_MAP_ENTRY(ISpxGetUspMessageParamsFromRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioReplayer)
        SPX_INTERFACE_MAP_ENTRY(ISpxTelemetryStore)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapterTokenProvider)
    SPX_INTERFACE_MAP_END()

    // --- ISpxObjectInit

    void Init() override;
    void Term() override;

    // --- ISpxAudioStreamSessionInit

    void InitFromFile(const char * fileName) override;
    void InitFromMicrophone() override;
    void InitFromStream(std::shared_ptr<ISpxAudioStream> stream) override;

    // --- ISpxAudioProcessor

    void SetFormat(const SPXWAVEFORMATEX* pformat) override;
    void ProcessAudio(const DataChunkPtr& audioChunk) override;

    // --- IServiceProvider ---

    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxRecoResultFactory)
        SPX_SERVICE_MAP_ENTRY(ISpxEventArgsFactory)
        SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
        SPX_SERVICE_MAP_ENTRY(ISpxSession)
        SPX_SERVICE_MAP_ENTRY(ISpxAudioPumpSite)
        SPX_SERVICE_MAP_ENTRY(ISpxTelemetryStore)
        SPX_SERVICE_MAP_ENTRY(ISpxGetUspMessageParamsFromUser)
        SPX_SERVICE_MAP_ENTRY(ISpxGetUspMessageParamsFromRecognizer)
        SPX_SERVICE_MAP_ENTRY(ISpxSpeechEventPayloadProvider)
        SPX_SERVICE_MAP_ENTRY(ISpxAudioReplayer)
        SPX_SERVICE_MAP_ENTRY(ISpxInteractionIdProvider)
        SPX_SERVICE_MAP_ENTRY(ISpxRecoEngineAdapterTokenProvider)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
        SPX_SERVICE_MAP_ENTRY_OBJECT(ISpxThreadService, m_threadService)
    SPX_SERVICE_MAP_END()

    // --- ISpxNamedProperties (overrides)
    SPX_NAMED_PROPERTY_MAP_BEGIN()
        SPX_NAMED_PROPERTY_MAP_ENTRY("KWSModelPath", PAL::ToString(m_kwsModel->GetFileName()))
        SPX_NAMED_PROPERTY_MAP_ENTRY_PROPID(Speech_SessionId, PAL::ToString(m_sessionId))
        SPX_NAMED_PROPERTY_MAP_FUNC(NamedProperties_Base::Match)
    SPX_NAMED_PROPERTY_MAP_END()

    // --- ISpxSession ---
    const std::wstring& GetSessionId() const override;

    void AddRecognizer(std::shared_ptr<ISpxRecognizer> recognizer) override;
    void RemoveRecognizer(ISpxRecognizer* recognizer) override;

    CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync() override;
    CSpxAsyncOp<void> StartContinuousRecognitionAsync() override;
    CSpxAsyncOp<void> StopContinuousRecognitionAsync() override;

    CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsyncWithVAD() override;
    CSpxAsyncOp<void> StartContinuousRecognitionAsyncWithVAD() override;
    CSpxAsyncOp<void> StopContinuousRecognitionAsyncWithVAD() override;

    CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync(std::shared_ptr<ISpxKwsModel> model) final;
    CSpxAsyncOp<void> StartKeywordRecognitionAsync(std::shared_ptr<ISpxKwsModel> model) override;
    CSpxAsyncOp<void> StopKeywordRecognitionAsync() override;

    CSpxAsyncOp<std::string> SendActivityAsync(std::string activity) final;

    void OpenConnection(bool forContinuousRecognition) override;
    void CloseConnection() override;
    void WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency) override;

    void SendSpeechEventMessage(std::string&& payload) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::string&& payload, bool alwaysSend) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload, bool alwaysSend) override;

    bool IsStreaming() override;

    void SetConversation(std::shared_ptr<ISpxConversation> conversation) override;
    void SetMeeting(std::shared_ptr<ISpxMeeting> meeting) override;
    void SetDisposing() override;

    // --- ISpxDetectorEngineAdapterSite
    void OnDetected(ISpxDetectorEngineAdapter* adapter, uint64_t offset, uint64_t duration, double confidence, const std::string& keyword, const DataChunkPtr& audioChunk) override;
    void AdapterCompletedSetFormatStop(ISpxDetectorEngineAdapter* /* adapter */) override { AdapterCompletedSetFormatStop(AdapterDoneProcessingAudio::Detection); }
    void GatingAdapterFireInitialSilenceTimeout() override;

    // --- ISpxSpeechAudioProcessorAdapterSite
    void SpeechStartDetected(uint64_t offset) override;
    void SpeechEndDetected(uint64_t offset) override;

    // --- ISpxRecoEngineAdapterSite (first part...)
    void GetScenarioCount(uint16_t* countSpeech, uint16_t* countTranslation, uint16_t* countDialog, uint16_t* countConversationTranscriber, uint16_t* countConversationTranscriberV2, uint16_t* countMeetingTranscriber, uint16_t* countLanguageId) override;

    std::list<std::string> GetListenForList() override;
    std::shared_ptr<ISpxRecognitionResult> GetSpottedKeywordResult() override;

    void AdapterStartingTurn(ISpxRecoEngineAdapter* adapter) override;
    void AdapterStartedTurn(ISpxRecoEngineAdapter* adapter, const std::string& id, OffsetType adapterStartOffset) override;
    void AdapterStoppedTurn(ISpxRecoEngineAdapter* adapter, bool) override;
    bool IsExpectingAdapterStoppedTurn(ISpxRecoEngineAdapter* adapter) override;

    void AdapterDetectedSpeechStart(ISpxRecoEngineAdapter* adapter, uint64_t offset) override;
    void AdapterDetectedSpeechEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset) override;
    void AdapterDetectedSoundStart(ISpxRecoEngineAdapter* adapter, uint64_t offset) override;
    void AdapterDetectedSoundEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset) override;
    void AdapterEndOfDictation(ISpxRecoEngineAdapter* adapter, uint64_t offset, uint64_t duration) override;

    // -- ISpxEventArgsFactory
    std::shared_ptr<ISpxSessionEventArgs> CreateSessionEventArgs(const std::wstring& sessionId) override;
    std::shared_ptr<ISpxConnectionEventArgs> CreateConnectionEventArgs(const std::wstring& sessionId) override;
    std::shared_ptr<ISpxConnectionMessageEventArgs> CreateConnectionMessageEventArgs(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary) override;
    std::shared_ptr<ISpxRecognitionEventArgs> CreateRecognitionEventArgs(const std::wstring& sessionId, uint64_t offset) override;
    std::shared_ptr<ISpxRecognitionEventArgs> CreateRecognitionEventArgs(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result) override;
    std::shared_ptr<ISpxActivityEventArgs> CreateActivityEventArgs(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) final;
    std::shared_ptr<ISpxTurnStatusEventArgs> CreateTurnStatusEventArgs(const std::string& interactionId, const std::string& conversationId, int status) final;
    std::shared_ptr<ISpxSessionEventArgs> CreateTokenRequestEventArgs(const std::wstring& sessionId) final;

    // --- ISpxRecoResultFactory
    std::shared_ptr<ISpxRecognitionResult> CreateIntermediateResult(const char* text, uint64_t offset, uint64_t duration, const char* phraseId) override;
    std::shared_ptr<ISpxRecognitionResult> CreateFinalResult(
        ResultReason reason,
        NoMatchReason noMatchReason,
        const char* text,
        uint64_t offset,
        uint64_t duration,
        const char* phraseId,
        const char* userId = nullptr) override;
    std::shared_ptr<ISpxRecognitionResult> CreateKeywordResult(const double confidence, const uint64_t offset, const uint64_t duration, const char* keyword, ResultReason reason, std::shared_ptr<ISpxAudioDataStream> stream) final;
    std::shared_ptr<ISpxRecognitionResult> CreateErrorResult(const std::shared_ptr<ISpxErrorInformation>& error) override;
    std::shared_ptr<ISpxRecognitionResult> CreateEndOfStreamResult() override;

    // --- ISpxRecoEngineAdapterSite (second part...)
    void FireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override;
    void FireAdapterResult_KeywordResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isAccepted) override;
    void FireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override;
    void FireAdapterResult_ActivityReceived(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) final;
    void FireAdapterResult_TurnStatusReceived(std::wstring interactionId, std::string conversationId, int statusCode) final;
    void FireAdapterResult_TranslationSynthesis(std::shared_ptr<ISpxRecognitionResult> result) override;
    void AdapterConnected(const std::string& url) override;
    void AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload) override;
    void FireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary) override;

    void AdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* /* adapter */) override { AdapterCompletedSetFormatStop(AdapterDoneProcessingAudio::Speech); }
    void AdapterRequestingAudioMute(ISpxRecoEngineAdapter* adapter, bool muteAudio) override;

    void AdditionalMessage(ISpxRecoEngineAdapter* adapter, uint64_t offset, AdditionalMessagePayload_Type payload) override;
    void Error(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload) override;

    // --- ISpxAudioPumpSite
    void Error(const std::string& error) override;

    // --- ISpxRecognizerSite
    std::shared_ptr<ISpxSession> GetDefaultSession() override;

    // --- ISpxSpeechEventPayloadProvider
    std::string GetSpeechEventPayload(bool startMeeting) override;

    // --- ISpxGetUspMessageParamsFromUser
    CSpxStringMap GetParametersFromUser(std::string&& path) override;

    // --- ISpxGetUspMessageParamsFromRecognizer
    CSpxStringMap GetParametersFromRecognizer(std::string&& path) override;

    // --- ISpxAudioReplayer
    void ShrinkReplayBuffer(uint64_t newBaseOffset) override;
    void GetCurrentAudioBufferOffset(uint64_t* offsetInTicks, uint64_t* offsetInBytes) override;
    void GetCurrentAudioContinuationOffset(uint64_t* offsetInTicks) override;
    void GetMultiChannelProcessingMode(bool* useMultiChannelProcessing) override;

    // --- ISpxRecoEngineAdapterTokenProvider
    void RefreshToken() final;

    // --- NamedProperties_Base
    void SetStringValue(const char* name, const char* value) override final;


private:
    void CheckError(const std::string& error);

    DISABLE_COPY_AND_MOVE(CSpxAudioStreamSession);

    enum class RecognitionKind
    {
        Idle = 0,
        Detection = 1,
        DetectionSingleShot = 2,
        SingleShot = 3,
        Continuous = 4,
        DetectionOnce = 5,
        DetectionOnceSingleShot = 6
    };

    enum class SessionState
    {
        Idle = 0,
        WaitForPumpSetFormatStart = 1,
        ProcessingAudio = 2,
        HotSwapPaused = 3,
        StoppingPump = 4,
        WaitForAdapterCompletedSetFormatStop = 5,
        ProcessingAudioLeftovers = 6
    };

    CSpxAsyncOp<void> StartRecognitionAsync(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model = nullptr);
    CSpxAsyncOp<void> StopRecognitionAsync(RecognitionKind stopKind);

    void StartRecognizing(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model = nullptr);
    void StopRecognizing(RecognitionKind stopKind);

    void WaitForRecognition_Complete(std::shared_ptr<ISpxRecognitionResult> result);

    void EnsureFireSessionStarted();
    void EnsureFireSessionStopped();

    void FireSpeechStartDetectedEvent(uint64_t offset);
    void FireSpeechEndDetectedEvent(uint64_t offset);

    void EnsureFireResultEvent();
    void FireResultEvent(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result);

    enum EventType
    {
        SessionStart,
        SessionStop,
        SpeechStart,
        SpeechEnd,
        RecoResultEvent,
        ActivityReceivedEvent,
        TurnStatusEvent,
        Connected,
        Disconnected,
        TokenRequest
    };

    void FireEvent(
        EventType sessionType,
        std::shared_ptr<ISpxRecognitionResult> result = nullptr,
        const wchar_t* sessionId = nullptr,
        uint64_t offset = 0,
        std::string activity = std::string{},
        int statusCode = 0,
        std::shared_ptr<ISpxAudioOutput> audio = nullptr);

    std::shared_ptr<ISpxRecognitionResult> CreateFakeFinalResult(const std::shared_ptr<ISpxRecognitionResult>& intermediate);


private:
    std::packaged_task<void()> CreateTask(std::function<void()> func, bool catchAll = true);
    std::shared_ptr<ISpxRecoEngineAdapter> EnsureInitRecoEngineAdapter();
    std::shared_ptr<ISpxRecoEngineAdapter> EnsureInitOutputEngineAdapter();
    std::shared_ptr<ISpxSpeechAudioProcessorAdapter> EnsureInitSpeechProcessor();
    void InitRecoEngineAdapter();

    void StartResetEngineAdapter();
    void EnsureResetEngineEngineAdapterComplete();

    std::shared_ptr<ISpxDetectorEngineAdapter> EnsureInitDetectionEngineAdapter(std::shared_ptr<ISpxKwsModel> model);
    void InitDetectionEngineAdapter(std::shared_ptr<ISpxKwsModel> model);

    std::shared_ptr<ISpxRecoEngineAdapter> EnsureInitMultiKeywordRecoAdapter(std::shared_ptr<ISpxKwsModel> model);

    bool ProcessNextAudio();

    void HotSwapToDetectionSingleShotWhilePaused(std::shared_ptr<ISpxRecognitionResult> spottedKeywordResult);

    void StartAudioPump(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model);
    void HotSwapAdaptersWhilePaused(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model = nullptr);

    void InformAdapterSetFormatStarting(const SPXWAVEFORMATEX* format);
    void InformAdapterSetFormatStopping(SessionState comingFromState);
    void EncounteredEndOfStream();

    enum AdapterDoneProcessingAudio { Detection, Speech };
    void AdapterCompletedSetFormatStop(AdapterDoneProcessingAudio doneAdapter);

    bool SessionIsIdle() const { return m_recoKind == RecognitionKind::Idle && m_sessionState == SessionState::Idle; }
    inline bool CanChangeConnection() const { return m_recoKind == RecognitionKind::Detection || m_sessionState == SessionState::Idle; }

    bool TryChangeState(std::initializer_list<RecognitionKind> validOriginKinds, std::initializer_list<SessionState> validOriginStates, RecognitionKind targetKind, SessionState targetState);
    inline bool TryChangeState(std::initializer_list<SessionState> validOriginStates, SessionState targetState)
    {
        return TryChangeState({ m_recoKind }, validOriginStates, m_recoKind, targetState);
    }
    inline bool TryChangeState(SessionState validOriginState, RecognitionKind targetKind, SessionState targetState)
    {
        return TryChangeState({ m_recoKind }, { validOriginState }, targetKind, targetState);
    }
    inline bool TryChangeState(SessionState validOriginState, SessionState targetState)
    {
        return TryChangeState({ validOriginState }, targetState);
    }

    bool CurrentStateMatches(std::initializer_list<RecognitionKind> allowedKinds, std::initializer_list<SessionState> allowedStates)
    {
        return std::any_of(allowedKinds.begin(), allowedKinds.end(), [this](const RecognitionKind allowedKind) { return m_recoKind == allowedKind; })
            && std::any_of(allowedStates.begin(), allowedStates.end(), [this](const SessionState allowedState) { return m_sessionState == allowedState; });
    }
    inline bool CurrentStateMatches(std::initializer_list<RecognitionKind> allowedKinds) { return CurrentStateMatches(allowedKinds, { m_sessionState }); }
    inline bool CurrentStateMatches(std::initializer_list<SessionState> allowedStates) { return CurrentStateMatches({ m_recoKind }, allowedStates); }

    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override;

    void WaitForIdle(std::chrono::milliseconds timeout);
    void Ensure16kHzSampleRate();
    void CancelPendingSingleShot(RecognitionKind kind);
    void SlowDownThreadIfNecessary(uint32_t size, std::chrono::milliseconds nonAcknowledgedSizeInMsec);
    void DispatchEvent(const std::list<std::weak_ptr<ISpxRecognizer>>& weakRecognizers,
                       const std::wstring& sessionId, EventType sessionType, uint64_t offset,
                       std::shared_ptr<ISpxRecognitionResult> result,
                       std::string activity, int statusCode,
                       std::shared_ptr<ISpxAudioOutput> audio);

    struct Operation;
    void RecognizeOnceAsync(const std::shared_ptr<Operation>& singleShot, std::shared_ptr<ISpxKwsModel> model = nullptr);

    void SetAudioConfigurationInProperties();
    void WriteTracingEvent();

    uint64_t CalculateLatencyInMs(const ProcessedAudioTimestampPtr& audiotimestamp) const;
    uint64_t CalculateResultLatencyInMs(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isFinalResult, bool isFirstHypothesis);

    std::shared_ptr<ISpxRecognitionResult> ApplyLatencyThreshold(uint64_t latencyMs, std::shared_ptr<ISpxRecognitionResult> result, bool isFinalResult);
    void EvaluateMaxLatencyReconnect();
    std::shared_ptr<ISpxRecognitionResult> UpdateResultLatency(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isFinalResult, bool isFirstHypothesis);
    void UpdateResultWithDetectionOffset(std::shared_ptr<ISpxRecognitionResult> result);

    void SetThrottleVariables(const SPXWAVEFORMATEX* format);

    void ForEachRecognizer(std::function<void(std::shared_ptr<ISpxRecognizer>)> fn);
    void ScheduleTokenRefresh();
    void EnsureValidToken();
    std::packaged_task<void()> GetTokenRefreshTask();

    template<typename T>
    CSpxAsyncOp<bool> SendMessageToService(const char *path, T&& payload, bool alwaysSend);

    bool IsUsingRecoEngineRnnt()
    {
        const auto recoBackend = GetOr<std::string>(PropertyId::SpeechServiceConnection_RecoBackend, std::string());
        return (recoBackend == "offline");
    };

private:

    std::shared_ptr<ISpxGenericSite> m_siteKeepAlive;

    // Unique identifier of the session, used mostly for diagnostics.
    // Is represented by UUID without dashes.
    const std::wstring m_sessionId;

    using milliseconds = std::chrono::milliseconds;
    using seconds = std::chrono::seconds;
    using minutes = std::chrono::minutes;

    //  To orchestrate the conversion of "Audio Data" into "Results" and "Events", we'll use utilize
    //  one "Audio Pump" and multiple "Adapters"

    SpxWAVEFORMATEX_Type m_format;
    std::mutex m_formatMutex;
    std::shared_ptr<ISpxAudioSessionShim> m_audioShim;
    CSpxSaveToWavFile m_saveToWavEverything;
    CSpxSaveToWavFile m_saveToWavDetectedKeyword;

    std::shared_ptr<ISpxDetectorEngineAdapter> m_detectionAdapter;
    std::shared_ptr<ISpxKwsModel> m_kwsModel;

    std::shared_ptr<ISpxRecoEngineAdapter> m_recoAdapter;

    std::shared_ptr<ISpxAudioStreamReader> m_codecAdapter;
    std::shared_ptr<CSpxCompressedAudioAdapter> m_compressedAudioAdapter = nullptr;

    std::shared_ptr<ISpxRecoEngineAdapter> m_multiKeywordRecoAdapter;

    // Our current "state" is kept in two parts and can only be changed from the background thread.
    //      1.) RecognitionKind (m_recoKind): Keeps track of what kind of recognition we're doing
    //      2.) SessionState (m_sessionState): Keeps track of what we're doing with Audio data
    // We still currently notify about the state change using the below conditional variable.
    // (TODO: this will be removed, but currently RecognizeAsync has to wait till the session
    // is in a clear state in order the next RecognizeAsync to succeed.)
    std::mutex m_stateMutex;
    std::condition_variable m_cv;

    RecognitionKind m_recoKind;
    SessionState m_sessionState;

    bool m_sawEndOfStream;      // Flag indicating that we have processed all data and got response from the service.
    bool m_fireEndOfStreamAtSessionStop;

    bool m_adapterResetPending;
    bool m_adapterStreamingAudio;
    bool m_expectFirstHypothesis;

    bool m_adapterAudioMuted;
    bool m_audioPumpStoppedBeforeHotSwap;
    RecognitionKind m_turnEndStopKind;

    // In order to reliably deliver audio, we always swap audio processor
    // together with its audio buffer. Otherwise data can be processed by a stale processor
    std::shared_ptr<ISpxAudioProcessor> m_audioProcessor;
    std::shared_ptr<ISpxAudioProcessor> m_speechProcessor;

    /*
     * Since we are adding the KeywordOnce kind, we are expanding this variable to give us
     * information on the source kind so we can go back to it after a KWSSingleShot
     */
    enum class DetectionProcessorMode
    {
        None = 0,
        Continuous,
        SingleShot
    };

    inline static DetectionProcessorMode DetectionProcessorModeFromKind(RecognitionKind kind) noexcept
    {
        if (kind == RecognitionKind::Detection)
        {
            return DetectionProcessorMode::Continuous;
        }
        else if (kind == RecognitionKind::DetectionOnce)
        {
            return DetectionProcessorMode::SingleShot;
        }
        return DetectionProcessorMode::None;

    }
    inline static RecognitionKind OriginKindFromDetectionMode(DetectionProcessorMode mode) noexcept
    {
        /* This is not totally accurate as we return Keyword for anything that is not single shot
         * but given that this is only called in the context of keyword it will be ok
         */
        return mode == DetectionProcessorMode::SingleShot ? RecognitionKind::DetectionOnce : RecognitionKind::Detection;
    }

    DetectionProcessorMode m_detectionProcessorMode;
    AudioBufferPtr m_audioBuffer;
    DataChunkPtr m_replayBuffer;
    AudioStreamSessionThrottleLogicPtr m_throttleLogic;

    // The minimum time that before the next audio frame should be processed.
    // This time is calculated as each audio frame is processed, and the processing
    // of the next frame will delay until this time is reached.
    std::chrono::steady_clock::time_point m_nextAudioProcessTime = std::chrono::steady_clock::now();
    SelfCorrectingSleeper m_audioProcessSleepWrapper;

    // In the event the system's steady_clock lacks the precision to throttle properly, fall back
    // to duration based where we will wait a % of the audio packet length.
    bool m_useDurationBasedThrottle = false;

    seconds GetStopRecognitionTimeout();

    std::list<std::weak_ptr<ISpxRecognizer>> m_recognizers;
    mutable std::mutex m_recognizersLock;

    std::weak_ptr<ISpxConversation> m_conversation;
    mutable std::mutex m_conversationLock;

    std::weak_ptr<ISpxMeeting> m_meeting;
    mutable std::mutex m_meetingLock;

    bool m_isReliableDelivery;
    uint64_t m_lastErrorGlobalOffset;
    uint64_t m_currentTurnGlobalOffset;

    uint64_t m_bytesTransited;

    ISpxThreadService::Ptr m_threadService;

    struct Operation
    {
        // The below static constant is set to 1 minute as timeout value for
        // RecognizeAsync()/SingleShot for KWS,
        // which should be sufficient even for translation in conversation mode.
        // Note using std::chrono::minutes::max() could cause wait_for to exit straight away instead of
        // infinite timeout, because wait_for() in VS is implemented via wait_until() and a possible integer
        // overflow could make new time < now.
        const static minutes Timeout;
        static std::atomic<int64_t> OperationId;

        explicit Operation(RecognitionKind kind) : m_operationId{ OperationId++ }, m_kind{ kind }
        {
            m_future = std::shared_future<std::shared_ptr<ISpxRecognitionResult>>(m_promise.get_future());
        }

        const int64_t m_operationId;
        const RecognitionKind m_kind;
        std::promise<std::shared_ptr<ISpxRecognitionResult>> m_promise;
        std::shared_future<std::shared_ptr<ISpxRecognitionResult>> m_future;

        // Details about the spotted keyword to verify.
        std::shared_ptr<ISpxRecognitionResult> m_spottedKeywordResult;
    };

    // Single shot in flight operation.
    std::shared_ptr<Operation> m_singleShotInFlight;
    std::shared_ptr<Operation> m_singleTextInFlight;
    std::shared_ptr<Operation> m_originalSingleShotInFlight;
    std::atomic<bool> m_sessionActive;
    std::atomic<bool> m_isDisposing;
    std::mutex m_stopMutex;
    std::condition_variable m_stopCondVar;
    std::atomic<bool> m_sessionStarted;
    std::atomic<bool> m_sessionStopped;

    // For proper segmentation with keyword in offline (RNN-T) reco
    std::string m_keyword;

    // For adding back VAD gated offset back to final recognized result
    uint64_t m_GatedOffset;

    std::shared_ptr<ISpxRecognitionResult> m_lastLowLatencyIntermediate;

    // To indicate Error() has been called and operation canceled
    std::atomic<bool> m_canceledOnError;

    std::atomic<bool> m_isMultiKeywordRecognition;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
