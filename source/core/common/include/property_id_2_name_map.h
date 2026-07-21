//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// property_id_2_name_map.h: internal mapping function from id to its name
//

#pragma once

#include "speechapi_cxx_enums.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

constexpr const char* GetPropertyName(const PropertyId id)
{
    switch (id)
    {
    case PropertyId::SpeechServiceConnection_Key: return "SPEECH-SubscriptionKey";
    case PropertyId::SpeechServiceConnection_Endpoint: return "SPEECH-Endpoint";
    case PropertyId::SpeechServiceConnection_Host: return "SPEECH-Host";
    case PropertyId::SpeechServiceConnection_Region: return "SPEECH-Region";
    case PropertyId::SpeechServiceAuthorization_Token: return "SPEECH-AuthToken";
    case PropertyId::SpeechServiceAuthorization_Type: return "SpeechServiceAuthorization_Type";
    case PropertyId::SpeechServiceConnection_EndpointId: return "SPEECH-ModelId";
    case PropertyId::SpeechServiceConnection_ProxyHostName: return "SPEECH-ProxyHostName";
    case PropertyId::SpeechServiceConnection_ProxyPort: return "SPEECH-ProxyPort";
    case PropertyId::SpeechServiceConnection_ProxyUserName: return "SPEECH-ProxyUserName";
    case PropertyId::SpeechServiceConnection_ProxyPassword: return "SPEECH-ProxyPassword";
    case PropertyId::SpeechServiceConnection_Url: return "SPEECH-ConnectionUrl";
    case PropertyId::SpeechServiceConnection_ProxyHostBypass: return "SPEECH-ProxyHostBypass";
    case PropertyId::SpeechServiceConnection_TranslationToLanguages: return "TRANSLATION-ToLanguages";
    case PropertyId::SpeechServiceConnection_TranslationCategoryId: return "TRANSLATION-CategoryId";
    case PropertyId::SpeechServiceConnection_TranslationVoice: return "TRANSLATION-Voice";
    case PropertyId::SpeechServiceConnection_TranslationFeatures: return "TRANSLATION-Features";
    case PropertyId::SpeechServiceConnection_RecoMode: return "SPEECH-RecoMode";
    case PropertyId::SpeechServiceConnection_RecoLanguage: return "SPEECH-RecoLanguage";
    case PropertyId::SpeechServiceConnection_RecoBackend: return "SPEECH-RecoBackend";
    case PropertyId::SpeechServiceConnection_RecoModelName: return "SPEECH-RecoModelName";
    case PropertyId::SpeechServiceConnection_RecoModelKey: return "SPEECH-RecoModelKey";
    case PropertyId::SpeechServiceConnection_RecoModelIniFile: return "SPEECH-RecoModelIniFile";
    case PropertyId::SpeechServiceConnection_SynthLanguage: return "SPEECH-SynthLanguage";
    case PropertyId::SpeechServiceConnection_SynthVoice: return "SPEECH-SynthVoice";
    case PropertyId::SpeechServiceConnection_SynthOutputFormat: return "SPEECH-SynthOutputFormat";
    case PropertyId::SpeechServiceConnection_SynthEnableCompressedAudioTransmission: return "SPEECH-SynthEnableCompressedAudioTransmission";
    case PropertyId::SpeechServiceConnection_SynthBackend: return "SPEECH-SynthesisBackend";
    case PropertyId::SpeechServiceConnection_SynthOfflineDataPath: return "SPEECH-SynthesisOfflineDataPath";
    case PropertyId::SpeechServiceConnection_SynthOfflineVoice: return "SPEECH-SynthesisOfflineVoice";
    case PropertyId::SpeechServiceConnection_SynthModelKey: return "SPEECH-SynthesisModelKey";
    case PropertyId::SpeechServiceConnection_VoicesListEndpoint: return "SPEECH-VoicesListEndpoint";
    case PropertyId::SpeechServiceConnection_UserDefinedQueryParameters: return "SPEECH-UserDefinedQueryParameters";
    case PropertyId::Speech_SessionId: return "SessionId";
    case PropertyId::SpeechServiceConnection_InitialSilenceTimeoutMs: return "SPEECH-InitialSilenceTimeoutMs";
    case PropertyId::SpeechServiceConnection_EndSilenceTimeoutMs: return "SPEECH-EndSilenceTimeoutMs";
    case PropertyId::SpeechServiceConnection_EnableAudioLogging: return "SPEECH-EnableAudioLogging";
    case PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages: return "Auto-Detect-Source-Languages";
    case PropertyId::SpeechServiceConnection_AutoDetectSourceLanguageResult: return "Auto-Detect-Source-Language-Result";
    case PropertyId::SpeechServiceConnection_LanguageIdMode: return "SPEECH-LanguageIdMode";
    case PropertyId::SpeechServiceResponse_RequestDetailedResultTrueFalse: return "SpeechServiceResponse_RequestDetailedResultTrueFalse";
    case PropertyId::SpeechServiceResponse_RequestProfanityFilterTrueFalse: return "SpeechServiceResponse_RequestProfanityFilterTrueFalse";
    case PropertyId::SpeechServiceResponse_ProfanityOption: return "SpeechServiceResponse_ProfanityOption";
    case PropertyId::SpeechServiceResponse_PostProcessingOption: return "SpeechServiceResponse_PostProcessingOption";
    case PropertyId::SpeechServiceResponse_RequestWordLevelTimestamps: return "SpeechServiceResponse_RequestWordLevelTimestamps";
    case PropertyId::SpeechServiceResponse_StablePartialResultThreshold: return "SpeechServiceResponse_StablePartialResultThreshold";
    case PropertyId::SpeechServiceResponse_TranslationRequestStablePartialResult: return "SpeechServiceResponse_TranslationRequestStablePartialResult";
    case PropertyId::SpeechServiceResponse_OutputFormatOption: return "SpeechServiceResponse_OutputFormatOption";
    case PropertyId::SpeechServiceResponse_RequestSnr: return "SpeechServiceResponse_RequestSnr";
    case PropertyId::SpeechServiceResponse_RequestWordBoundary: return "SpeechServiceResponse_RequestWordBoundary";
    case PropertyId::SpeechServiceResponse_RequestPunctuationBoundary: return "SpeechServiceResponse_RequestPunctuationBoundary";
    case PropertyId::SpeechServiceResponse_RequestSentenceBoundary: return "SpeechServiceResponse_RequestSentenceBoundary";
    case PropertyId::SpeechServiceResponse_SynthesisEventsSyncToAudio: return "SpeechServiceResponse_SynthesisEventsSyncToAudio";
    case PropertyId::SpeechServiceResponse_JsonResult: return "RESULT-Json";
    case PropertyId::SpeechServiceResponse_JsonErrorDetails: return "RESULT-ErrorDetails";
    case PropertyId::SpeechServiceResponse_RecognitionLatencyMs: return "RESULT-RecognitionLatencyMs";
    case PropertyId::SpeechServiceResponse_RecognitionBackend: return "RESULT-RecognitionBackend";
    case PropertyId::SpeechServiceResponse_RequestId: return "RESULT-RequestId";
    case PropertyId::SpeechServiceResponse_SynthesisFirstByteLatencyMs: return "RESULT-SynthesisFirstByteLatencyMs";
    case PropertyId::SpeechServiceResponse_SynthesisFinishLatencyMs: return "RESULT-SynthesisFinishLatencyMs";
    case PropertyId::SpeechServiceResponse_SynthesisUnderrunTimeMs: return "RESULT-SynthesisUnderrunTimeMs";
    case PropertyId::SpeechServiceResponse_SynthesisConnectionLatencyMs: return "RESULT-SynthesisConnectionLatencyMs";
    case PropertyId::SpeechServiceResponse_SynthesisNetworkLatencyMs: return "RESULT-SynthesisNetworkLatencyMs";
    case PropertyId::SpeechServiceResponse_SynthesisServiceLatencyMs: return "RESULT-SynthesisServiceLatencyMs";
    case PropertyId::SpeechServiceResponse_SynthesisBackend: return "RESULT-SynthesisBackend";
    case PropertyId::SpeechServiceResponse_DiarizeIntermediateResults: return "SpeechServiceResponse_DiarizeIntermediateResults";
    case PropertyId::CancellationDetails_Reason: return "CancellationDetails_Reason";
    case PropertyId::CancellationDetails_ReasonText: return "CancellationDetails_ReasonText";
    case PropertyId::CancellationDetails_ReasonDetailedText: return "CancellationDetails_ReasonDetailedText";
    case PropertyId::AudioConfig_DeviceNameForCapture: return "AudioConfig_DeviceNameForCapture";
    case PropertyId::AudioConfig_NumberOfChannelsForCapture: return "AudioConfig_NumberOfChannelsForCapture";
    case PropertyId::AudioConfig_SampleRateForCapture: return "AudioConfig_SampleRateForCapture";
    case PropertyId::AudioConfig_BitsPerSampleForCapture: return "AudioConfig_BitsPerSampleForCapture";
    case PropertyId::AudioConfig_AudioSource: return "AudioConfig_AudioSource";
    case PropertyId::AudioConfig_DeviceNameForRender: return "AudioConfig_DeviceNameForRender";
    case PropertyId::AudioConfig_PlaybackBufferLengthInMs: return "AudioConfig_PlaybackBufferLengthInMs";
    case PropertyId::AudioConfig_AudioProcessingOptions: return "AudioConfig_AudioProcessingOptions";
    case PropertyId::AudioProcessing_EchoCancellationModelPath: return "EcModelFilePath";
    case PropertyId::AudioProcessing_PersonalizedNoiseSuppressionModelPath: return "PnsModelFilePath";
    case PropertyId::Speech_LogFilename: return "SPEECH-LogFilename";
    case PropertyId::Speech_SegmentationSilenceTimeoutMs: return "SPEECH-SegmentationSilenceTimeoutMs";
    case PropertyId::Speech_SegmentationMaximumTimeMs: return "SPEECH-SegmentationMaximumTimeMs";
    case PropertyId::Speech_SegmentationStrategy: return "SPEECH-SegmentationStrategy";
    case PropertyId::Speech_StartEventSensitivity: return "SPEECH-StartEventSensitivity";
    case PropertyId::Speech_EnableMultiChannelProcessing: return "SPEECH-EnableMultiChannelProcessing";
    case PropertyId::Conversation_ApplicationId: return "DIALOG-ApplicationId";
    case PropertyId::Conversation_DialogType: return "DIALOG-DialogType";
    case PropertyId::Conversation_Initial_Silence_Timeout: return "DIALOG-InitialSilenceTimeout";
    case PropertyId::Conversation_From_Id: return "DIALOG-FromId";
    case PropertyId::Conversation_Conversation_Id: return "DIALOG-ConversationId";
    case PropertyId::Conversation_Custom_Voice_Deployment_Ids: return "DIALOG-CustomVoiceDeploymentIds";
    case PropertyId::Conversation_Speech_Activity_Template: return "DIALOG-SpeechActivityTemplate";
    case PropertyId::Conversation_Request_Bot_Status_Messages: return "DIALOG-RequestBotStatusMessages";
    case PropertyId::Conversation_Connection_Id: return "DIALOG-ConnectionId";
    case PropertyId::DataBuffer_TimeStamp: return "DataBuffer_TimeStamp";
    case PropertyId::DataBuffer_UserId: return "DataBuffer_UserId";
    case PropertyId::PronunciationAssessment_ReferenceText: return "PronunciationAssessment_ReferenceText";
    case PropertyId::PronunciationAssessment_GradingSystem: return "PronunciationAssessment_GradingSystem";
    case PropertyId::PronunciationAssessment_Granularity: return "PronunciationAssessment_Granularity";
    case PropertyId::PronunciationAssessment_EnableMiscue: return "PronunciationAssessment_EnableMiscue";
    case PropertyId::PronunciationAssessment_PhonemeAlphabet: return "PronunciationAssessment_PhonemeAlphabet";
    case PropertyId::PronunciationAssessment_EnableProsodyAssessment: return "PronunciationAssessment_EnableProsodyAssessment";
    case PropertyId::PronunciationAssessment_NBestPhonemeCount: return "PronunciationAssessment_NBestPhonemeCount";
    case PropertyId::PronunciationAssessment_Json: return "PronunciationAssessment_Json";
    case PropertyId::PronunciationAssessment_Params: return "PronunciationAssessment_Params";
    case PropertyId::SpeechTranslation_ModelName: return "SpeechTranslation_ModelName";
    case PropertyId::SpeechTranslation_ModelKey: return "SpeechTranslation_ModelKey";
    case PropertyId::KeywordRecognition_ModelName: return "KeywordRecognition_ModelName";
    case PropertyId::KeywordRecognition_ModelKey: return "KeywordRecognition_ModelKey";
    case PropertyId::EmbeddedSpeech_EnablePerformanceMetrics: return "EmbeddedSpeech_EnablePerformanceMetrics";
    case PropertyId::SpeechSynthesisRequest_Pitch: return "SpeechSynthesisRequest_Pitch";
    case PropertyId::SpeechSynthesisRequest_Rate: return "SpeechSynthesisRequest_Rate";
    case PropertyId::SpeechSynthesisRequest_Volume: return "SpeechSynthesisRequest_Volume";
    case PropertyId::SpeechSynthesisRequest_Style: return "SpeechSynthesisRequest_Style";
    case PropertyId::SpeechSynthesisRequest_Temperature: return "SpeechSynthesisRequest_Temperature";
    case PropertyId::SpeechSynthesisRequest_CustomLexiconUrl: return "SpeechSynthesisRequest_CustomLexiconUrl";
    case PropertyId::SpeechSynthesisRequest_PreferLocales: return "SpeechSynthesisRequest_PreferLocales";
    case PropertyId::SpeechSynthesis_FrameTimeoutInterval: return "SpeechSynthesis_FrameTimeoutInterval";
    case PropertyId::SpeechSynthesis_RtfTimeoutThreshold: return "SpeechSynthesis_RtfTimeoutThreshold";

    default: return nullptr;
    }
}

constexpr inline const char* SpxGetPropertyName(const int id, const char* name)
{
    if (id == 0 && name != nullptr) return name;
    return GetPropertyName(static_cast<PropertyId>(id));
}

constexpr auto g_recoModeInteractive = "INTERACTIVE";
constexpr auto g_recoModeDictation = "DICTATION";
constexpr auto g_recoModeConversation = "CONVERSATION";

constexpr auto g_segmentationStrategyDefault = "Default";
constexpr auto g_segmentationStrategyTime = "Time";
constexpr auto g_segmentationStrategySemantic = "Semantic";

constexpr auto g_segmentationModeNormal = "Normal";
constexpr auto g_segmentationModeCustom = "Custom";
constexpr auto g_segmentationModeSemantic = "Semantic";

constexpr auto g_audioSourceMicrophone = "Microphones";
constexpr auto g_audioSourceStream = "Stream";
constexpr auto g_audioSourceFile = "File";

constexpr auto KeywordConfig_EnableKeywordVerification = "KeywordConfig_EnableKeywordVerification";
constexpr auto g_keyword_KeywordOnly = "IsKeywordRecognizer";
constexpr auto g_keyword_KeywordAndSpeech = "IsKeywordAndSpeechRecognizer";

constexpr auto g_audioContinuationOffset = "SPEECH-UspContinuationOffset";

constexpr auto g_Detection_VadModeOn = "IsVadModeOn";
constexpr auto g_Detection_ProcessingVAD = "IsProcessingVAD";

constexpr auto g_isConversationTranscriber = "IsConversationTranscriber";
constexpr auto g_isConversationTranscriber_V2 = "IsConversationTranscriber_V2";
constexpr auto g_isMeetingTranscriber = "IsMeetingTranscriber";
constexpr auto g_isDialogServiceConnector = "IsDialogServiceConnector";
constexpr auto g_isCustomV1Endpoint = "IsCustomV1Endpoint";
constexpr auto g_isUnifiedSpeechEndpoint = "IsUnifiedSpeechEndpoint";
constexpr auto g_unsupportedV2ServiceProperties = "UnsupportedV2ServiceProperties";

constexpr auto g_dialogType_BotFramework = "bot_framework";
constexpr auto g_dialogType_CustomCommands = "custom_commands";

constexpr auto g_autoDetectSourceLang_OpenRange = "UND";
constexpr auto g_propertyNameSeparator = "#";
constexpr auto g_queryParameterPropertyNamePrefix = "CARBON-INTERNAL-UserDefinedQueryParameters-";

// These values go into the `languageId` field of the speech.context JSON string:
constexpr auto g_languageIdModeDetectAtAudioStart = "DetectAtAudioStart";
constexpr auto g_languageIdModeDetectContinuous = "DetectContinuous";
constexpr auto g_languageIdModeDetectSegments = "DetectSegments";
constexpr auto g_languageIdPriorityPrioritizeAccuracy = "PrioritizeAccuracy";
constexpr auto g_languageIdPriorityPrioritizeLatency = "PrioritizeLatency";

// These are LID mode string properties that can be set by the application:
constexpr auto g_languageIdModeAtStart = "AtStart";
constexpr auto g_languageIdModeContinuous = "Continuous";
constexpr auto g_languageIdModeAtStartHighAccuracy = "AtStartHighAccuracy";

// For (embedded) multi-keyword recognition.
constexpr auto g_isMultiKeywordRecognition = "IsMultiKeywordRecognition";
constexpr auto g_keywordRecognitionModelPath = "KeywordRecognition_ModelPath";
constexpr auto g_keywordRecognitionUserDefinedWakeWords = "KeywordRecognition_UserDefinedWakeWords";

constexpr auto g_phraseListWeightPropertyName = "SPEECH-PhraseListWeight";
constexpr auto g_stopRecognitionTimeoutPropertyName = "SPEECH-StopRecognitionTimeoutInSeconds";

constexpr auto g_imageWidth = "ImageFormat_width";
constexpr auto g_imageHeight = "ImageFormat_height";
constexpr auto g_imageStride = "ImageFormat_stride";
constexpr auto g_imagePixelFormat = "ImageFormat_pixelFormat";

namespace Media
{
    namespace Frame
    {
        constexpr auto Timestamp = "frame.timestamp";
        constexpr auto StreamId = "frame.stream_id";
        constexpr auto ContentType = "frame.content_type";
    }

    namespace Adapter
    {
        namespace Streams
        {
            constexpr auto Count = "adapter.streams.count";
        }

        namespace Format
        {
            constexpr auto IsEncoded = "adapter.format.is_encoded";
            constexpr auto BitsPerSample = "adapter.format.bits_per_sample";
            constexpr auto NumberOfChannels = "adapter.format.number_of_channels";
            constexpr auto SamplesPerSec = "adapter.format.samples_per_sec";
        }

        namespace Audio
        {
            constexpr auto FrameSizeMS = "adapter.audio.frame_size_ms";
        }

        constexpr auto ReadTimeout = "adapter.read_timeout";
        constexpr auto Passthrough = "adapter.passthrough";
    }
    namespace Source
    {
        namespace Device
        {
            constexpr auto Attributes = "source.device.attributes";
            constexpr auto Type = "source.device.type";
        }

        namespace File
        {
            constexpr auto Name = "source.file.name";
            constexpr auto Type = "source.file.type";
            constexpr auto MaxSize = "source.file.max.size";
            constexpr auto NameNeededEventName = "source.file.name.needed";
            constexpr auto Multiple = "source.file.multi";
        }

        namespace Url
        {
            constexpr auto Name = "source.url.name";
            constexpr auto Type = "source.url.type";
        }
    }
    namespace Buffer
    {
        constexpr auto Size = "media.buffer.size.frames";
    }
    namespace Device
    {
        namespace K4A
        {
            constexpr auto MockFile = "media.device.k4a.mock_file";
        }
    }
}

namespace Network
{
    namespace USP
    {
        constexpr auto ConnectionId = "reco.engine.adapter.connection_id";
        constexpr auto MaxRetries = "usp.client.retries";
        constexpr auto RetransmitBufferSize = "usp.client.buffer.size:messages";
    }
}

}}}}
