//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// usp_reco_engine_adapter.h: Implementation declarations for CSpxUspRecoEngineAdapter C++ class
//

#pragma once
#include <ajv.h>
#include <memory>
#include <chrono>
#include <map>
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "recognition_result.h"
#include "service_helpers.h"
#include "audio_file_logger.h"
#include "usp.h"
#include "activity_session.h"
#include <object_with_site_init_impl.h>
#include "interfaces/ispx_usp_connection.h"
#include "save_to_wav.h"

#ifdef _MSC_VER
#include <shared_mutex>
#endif // _MSC_VER

#define KEYWORDS_PROPERTY_NAME "SPEECH-KeywordsToDetect"

class CSpxUspRecoEngineAdapterTest;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxUspRecoEngineAdapter :
    public ISpxObjectWithSiteInitImpl<ISpxRecoEngineAdapterSite>,
    public ISpxServiceProvider,
    public ISpxGenericSite,
    public USP::ISpxUspCallbacks,
    public ISpxRecoEngineAdapter,
    public ISpxPropertyBagImpl,
    public ISpxActivityResultAdapter
{
public:

    CSpxUspRecoEngineAdapter();
    ~CSpxUspRecoEngineAdapter();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        using namespace USP;
        SPX_INTERFACE_MAP_ENTRY(ISpxUspCallbacks)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapter)
        SPX_INTERFACE_MAP_ENTRY(ISpxActivityResultAdapter)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessor)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()


    // --- ISpxObject
    void Init() override;
    void Term() override;

    // --- ISpxRecoEngineAdapter
    void SetAdapterMode(bool singleShot) override;
    void OpenConnection(bool singleShot) override;
    void CloseConnection() override;
    void WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency) override;
    void FlushTelemetry() override;
    void SendAgentMessage(const std::string &buffer) final;
    void SendSpeechEventMessage(std::string&& msg) override;
    void SendNetworkMessage(const char* path, std::string&& msg, const std::shared_ptr<std::promise<bool>>& pr) override;
    void SendNetworkMessage(const char* path, std::vector<uint8_t>&& msg, const std::shared_ptr<std::promise<bool>>& pr) override;

    // --- ISpxAudioProcessor
    void SetFormat(const SPXWAVEFORMATEX* pformat) override;
    void ProcessAudio(const DataChunkPtr& audioChunk) override;

    // --- IServiceProvider ---
    SPX_SERVICE_MAP_BEGIN()
    SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

private:
    using SitePtr = std::shared_ptr<ISpxRecoEngineAdapterSite>;

    DISABLE_COPY_AND_MOVE(CSpxUspRecoEngineAdapter);

    void EnsureUspInit();
    void UspInitialize();
    void UspTerminate();
    void ResolveRecoMode(bool singleShot);

    void UserAgentInit();
    void AddDefaultTrafficType();

    USP::ClientConfiguration& SetUspEndpoint(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspEndpointTranslation(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspEndpointDefaultSpeechService(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspEndpointDialog(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspEndpointLanguageId(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspEndpointTranscriber(USP::ClientConfiguration& client, bool isDynamic);
    USP::ClientConfiguration& SetUspEndpointTranscriberV2(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspRegion(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspRecoMode(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspLanguageIdModeAndPriority(USP::ClientConfiguration& client, bool isStandaloneLid);
    USP::ClientConfiguration& SetUspAuthentication(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUserDefinedHttpHeaders(USP::ClientConfiguration& client);
    USP::ClientConfiguration& SetUspProxyInfo(USP::ClientConfiguration& client);
#if SPEECHSDK_USE_OPENSSL
    USP::ClientConfiguration& SetUspCrlProperties(USP::ClientConfiguration& client);
#endif
    template<size_t N>
    USP::ClientConfiguration& SetUspQueryParameters(const std::array<const char*, N>& allowedParameterList, USP::ClientConfiguration& client);
    void SetSingleUspQueryParameter(const char* queryParamName, USP::ClientConfiguration& client) const;

    void UpdateOutputFormatOption();
    void UpdateDefaultLanguage();
    USP::RecognitionMode GetRecoModeFromProperties() const;
    void UpdateLanguageIdModeAndPriorityFromProperties(USP::LanguageIdMode& languageIdMode, USP::LanguageIdPriority& languageIdPriority) const;

    void SetSpeechConfigMessage();

    void ProcessAudioFormat(SPXWAVEFORMATEX* pformat);
    void ProcessAudioChunk(const DataChunkPtr& audioChunk);
    void UspSendSpeechConfig();
    void UspSendAgentConfig();
    void UspSendSpeechContext();
    void UspSendSpeechEvent();
    void UspSendSpeechAgentContext();
    void UspSendMessage(const char *messagePath, const std::string &buffer, USP::MessageType messageType);
    void UspSendMessage(std::unique_ptr<USP::Message> message);
    void UspWriteActual(const DataChunkPtr& audioChunk);
    void FlushAudio(bool flushCodec = false);

    void OnMessageReceived(const USP::RawMsg&) override;
    void OnSpeechStartDetected(const USP::SpeechStartDetectedMsg&) override;
    void OnSpeechEndDetected(const USP::SpeechEndDetectedMsg&) override;
    void OnSpeechHypothesis(const USP::SpeechHypothesisMsg&) override;
    void OnSpeechFragment(const USP::SpeechFragmentMsg&) override;
    void OnSpeechKeywordDetected(const USP::SpeechKeywordDetectedMsg&) override;
    void OnSpeechPhrase(const USP::SpeechPhraseMsg&) override;
    void OnTurnStart(const USP::TurnStartMsg&) override;
    void OnTurnEnd(const USP::TurnEndMsg&) override;
    void OnMessageStart(const USP::TurnStartMsg&) final;
    void OnMessageEnd(const USP::TurnEndMsg&) final;
    void OnError(const std::shared_ptr<ISpxErrorInformation>& error) override;
    void OnUserMessage(const USP::UserMsg&) override;
    void OnConnected(const std::string&) override;
    void OnDisconnected(const std::shared_ptr<ISpxErrorInformation>&) override;
    void OnToken(const std::string token) override;
    void OnAcknowledgedAudio(uint64_t offset) override;

    void OnTranslationHypothesis(const USP::TranslationHypothesisMsg&) override;
    void OnTranslationPhrase(const USP::TranslationPhraseMsg&) override;
    void OnAudioOutputChunk(const USP::AudioOutputChunkMsg&) override;
    uint8_t* FormatBufferWriteBytes(uint8_t* buffer, const uint8_t* source, size_t bytes);
    uint8_t* FormatBufferWriteNumber(uint8_t* buffer, uint32_t number);
    uint8_t* FormatBufferWriteChars(uint8_t* buffer, const char* psz, size_t cch);
    uint32_t EndianConverter(uint32_t number)
    {
        return ((uint32_t)(number & 0x000000ff) << 24) |
               ((uint32_t)(number & 0x0000ff00) << 8) |
               ((uint32_t)(number & 0x00ff0000) >> 8) |
               ((uint32_t)(number & 0xff000000) >> 24);
    }

    std::string GetSpeechContextJson();
    bool IsUnifiedEndpoint();
    void AddModeJsonToContext(ajv::JsonBuilder& contextJson);
    void AddLanguageJsonToContext(ajv::JsonBuilder& contextJson);
    void AddDgiJsonToContext(ajv::JsonBuilder& contextJson);
    void AddKeywordDetectionJsonToContext(ajv::JsonBuilder& contextJson);
    void AddLeftRightJsonToContext(ajv::JsonBuilder& contextJson);
    void AddTranslationJsonToContext(ajv::JsonBuilder& contextJson);
    void AddLanguageIdJsonToContext(ajv::JsonBuilder& contextJson);
    void AddAudioJsonToContext(ajv::JsonBuilder& contextJson);
    void AddExtraPropertyJsonToContext(ajv::JsonBuilder& contextJson);
    void AddPronunciationJsonToContext(ajv::JsonBuilder& contextJson);
    void AddProfanityOptionJsonToContext(ajv::JsonBuilder& contextJson);
    void AddInitialSilenceTimeoutJsonToContext(ajv::JsonBuilder& contextJson);
    void AddEndSilenceTimeoutJsonToContext(ajv::JsonBuilder& contextJson);
    void AddStableIntermediateThresholdJsonToContext(ajv::JsonBuilder& contextJson);
    void AddPostProcessingOptionsJsonToContext(ajv::JsonBuilder& contextJson);
    void AddSegmentationJsonToContext(ajv::JsonBuilder& contextJson);
    void AddOutputDetailJsonToContext(ajv::JsonBuilder& contextJson);
    void AddOutputDetailLevelJsonToContext(ajv::JsonBuilder& contextJson);
    void AddConversationTranscriptionJsonToContext(ajv::JsonBuilder& contextJson);
    void AddSpeechStartEventSensitivityJsonToContext(ajv::JsonBuilder& contextJson);

    std::vector<std::pair<std::string, std::string>> GetPerLanguageSetting(
        const std::vector<std::string>& languages,
        PropertyId propertyId);
    void FireActivityResult(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) override;
    void FireFinalResultNow(const USP::SpeechPhraseMsg& message);
    void FireFinalResultLater(const USP::SpeechPhraseMsg& message);

    ResultReason ToReason(RecognitionStatus uspRecognitionStatus);
    CancellationReason ToCancellationReason(RecognitionStatus uspRecognitionStatus);
    NoMatchReason ToNoMatchReason(RecognitionStatus uspRecognitionStatus);

    enum class AudioState { Idle = 0, Ready = 1, Sending = 2, Mute = 9 };

    enum class UspState {
        Error = -1,
        Idle = 0,
        WaitingForTurnStart = 1000,
        WaitingForPhrase = 1200,
        WaitingForTurnEnd = 2999,
        Terminating = 9998,
        Zombie = 9999
    };

    bool IsBadState() const { return IsState(UspState::Error) || IsState(UspState::Terminating) || IsState(UspState::Zombie); }
    bool IsState(AudioState state) const { return m_audioState == state; }
    bool IsState(UspState state) const { return m_uspState == state; }
    bool IsState(AudioState audioState, UspState uspState) const { return IsState(audioState) && IsState(uspState); }
    bool IsStateBetween(UspState state1, UspState state2) const { return m_uspState > state1 && m_uspState < state2; }
    bool IsStateBetweenIncluding(UspState state1, UspState state2) const { return m_uspState >= state1 && m_uspState < state2; }

    bool TryChangeState(UspState toUspState) { return TryChangeState(m_audioState, m_uspState, m_audioState, toUspState); }
    bool TryChangeState(UspState fromUspState, UspState toUspState) { return TryChangeState(m_audioState, fromUspState, m_audioState, toUspState); }
    bool TryChangeState(AudioState toAudioState) { return TryChangeState(m_audioState, m_uspState, toAudioState, m_uspState); }
    bool TryChangeState(AudioState fromAudioState, AudioState toAudioState) { return TryChangeState(fromAudioState, m_uspState, toAudioState, m_uspState); }
    bool TryChangeState(AudioState toAudioState, UspState toUspState) { return TryChangeState(m_audioState, m_uspState, toAudioState, toUspState); }
    bool TryChangeState(AudioState fromAudioState, UspState fromUspState, AudioState toAudioState, UspState toUspState);

    SPXHR PrepareCompressionCodec(const SPXWAVEFORMATEX* format, ISpxInternalAudioCodecAdapter::SPXCompressedDataCallback dataCallback);
    void HandleCompressedAudioData(const uint8_t* outData, size_t nBytesOut);

    void PrepareFirstAudioReadyState(const SPXWAVEFORMATEX* format);
    void PrepareAudioReadyState();
    void PrepareUspAudioStream();
    void SendPreAudioMessages();

    bool ShouldResetAfterError();
    void ResetAfterError();

    bool ShouldResetAfterTurnStopped();
    void ResetAfterTurnStopped();

    bool ShouldResetBeforeFirstAudio();
    void ResetBeforeFirstAudio();

    void CreateConversationResult(std::shared_ptr<ISpxRecognitionResult>& result, const std::string& userId, const std::string& utteranceId);
    void UpdateAdapterResult_JsonResult(std::shared_ptr<ISpxRecognitionResult> result);

    DataChunkPtr MakeDataChunkForAudioFormat(SPXWAVEFORMATEX* pformat);

    CSpxStringMap GetParametersFromUser(std::string&& path);
    CSpxStringMap GetParametersFromRecognizer(std::string&& path);

    USP::MessageType GetMessageType(const std::string& path);

    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override;
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;
    static const std::vector<PropertyId> GetPropertyIdAliasesForQueryStrings(const PropertyId sourcePropertyId);
    std::string SetRecoMode(ajv::JsonBuilder& contextJson);

private:
    friend CSpxActivitySession;
    friend CSpxUspRecoEngineAdapterTest;

    static constexpr auto s_defaultRecognitionLanguage = "en-us";

    std::shared_ptr<ISpxUspCallbacks> m_uspCallbacks;
    ISpxUspConnection::Ptr m_uspConnection;

    bool m_continueOnKeywordReject = false;
    bool m_ignoreTelemetry = false;
    bool m_isInteractiveMode = false;
    std::string m_speechConfig{};
    bool m_customEndpoint = false;
    bool m_customHost = false;
    USP::EndpointType m_endpointType = USP::EndpointType::Speech;
    USP::LanguageIdMode m_languageIdMode = USP::LanguageIdMode::DetectAtAudioStart;
    USP::LanguageIdPriority m_languageIdPriority = USP::LanguageIdPriority::PrioritizeLatency;

    std::string m_continuationToken;
    std::string m_tokenOffset;
    std::string m_currentServiceTag;
    std::string m_currentRequestId;
    uint64_t m_startingOffset;

    bool m_allowUspResetAfterAudioByteCount = true;
    size_t m_resetUspAfterAudioSeconds = 2 * 60; // 2 minutes
    uint64_t m_resetUspAfterAudioByteCount;
    uint64_t m_uspAudioByteCount;

    bool m_allowUspResetAfterTime = true;
    size_t m_resetUspAfterTimeSeconds = 4 * 60; // 4 minutes
    std::chrono::system_clock::time_point m_uspInitTime;
    std::chrono::system_clock::time_point m_uspResetTime;

    const bool m_allowUspResetAfterError = true;
    bool m_singleShot = false;
    SpxWAVEFORMATEX_Type m_format;
    bool m_audioFormatSent = true;
    bool m_audioFlushed = false;
    std::shared_ptr<ISpxInternalAudioCodecAdapter> m_compressionCodec;
    AudioState m_audioState;
    UspState m_uspState;

    std::chrono::high_resolution_clock::time_point m_offlineTimestamp;

    USP::SpeechPhraseMsg m_finalResultMessageToFireLater;

    std::string m_dialogConversationId;

    std::map<std::string, std::unique_ptr<CSpxActivitySession>> m_request_session_map;

    std::map<std::string, USP::MessageType> m_message_name_to_type_map;

    CSpxSaveToWavFile m_saveToWavEverything;
    CSpxSaveToWavFile m_saveToWavCurrentTurn;

    bool m_handleAsSpeechV1Endpoint = false;
    bool m_useMultiChannelProcessing = false;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
