//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <functional>
#include <string>
#include <map>
#include <array>
#include <vector>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

struct endpoint
{
    static constexpr const char* protocol = "wss://";

    static constexpr const char* outputFormatQueryParam = "format=";
    static constexpr const char* langQueryParam = "language=";
    static constexpr const char* deploymentIdQueryParam = "cid=";
    static constexpr const char* profanityQueryParam = "profanity=";
    static constexpr const char* initialSilenceTimeoutQueryParam = "initialSilenceTimeoutMs=";
    static constexpr const char* endSilenceTimeoutQueryParam = "endSilenceTimeoutMs=";
    static constexpr const char* stableIntermediateThresholdQueryParam = "stableIntermediateThreshold=";
    static constexpr const char* storeAudioQueryParam = "storeAudio=";
    static constexpr const char* wordLevelTimestampsQueryParam = "wordLevelTimestamps=";
    static constexpr const char* wordLevelConfidenceQueryParam = "wordLevelConfidence=";

    static constexpr const char* outputFormatSimple = "simple";
    static constexpr const char* outputFormatDetailed = "detailed";

    static constexpr const char* postProcessingTrueText = "TrueText";

    static constexpr const char* profanityMasked = "masked";
    static constexpr const char* profanityRemoved = "removed";
    static constexpr const char* profanityRaw = "raw";

    static constexpr const char* tcpNodelayOption = "tcp_nodelay";

    struct unifiedspeech
    {
        static constexpr const char* hostnameSuffix = ".stt.speech.microsoft.com";
        static constexpr const char* path = "/stt/speech/universal/v2"; // This is the path the SDK will connect to.
        static constexpr const char* unifiedPath = "/speech/universal/v2"; // This covers "legacy" v2 paths that didn't use /stt/ in them.
        static constexpr const char* postprocessingQueryParam = "postprocessing=";
        static constexpr const char* lidEnabledQueryParam = "lidEnabled=";

        static constexpr std::array<const char*, 2> queryParameters{ {
            deploymentIdQueryParam,
            storeAudioQueryParam
        } };
    };

    struct v1speech
    {
        static constexpr const char* pathPrefix = "/speech/recognition/";
        static constexpr const char* pathSuffix = "/cognitiveservices/v1";
        static constexpr const char* postprocessingQueryParam = "postprocessing=";
        static constexpr const char* lidEnabledQueryParam = "lidEnabled=";

        static constexpr std::array<const char*, 12> queryParameters{ {
            langQueryParam,
            deploymentIdQueryParam,
            initialSilenceTimeoutQueryParam,
            endSilenceTimeoutQueryParam,
            storeAudioQueryParam,
            outputFormatQueryParam,
            wordLevelTimestampsQueryParam,
            wordLevelConfidenceQueryParam,
            profanityQueryParam,
            stableIntermediateThresholdQueryParam,
            postprocessingQueryParam,
            lidEnabledQueryParam
        } };
    };

    struct standalonelid
    {
        static constexpr const char* hostnameSuffix = ".stt.speech.microsoft.com";
        static constexpr const char* path = "/stt/speech/universal/v2";

        static constexpr std::array<const char*, 4> queryParameters{ {
            langQueryParam,
            outputFormatQueryParam,
            wordLevelTimestampsQueryParam,
            wordLevelConfidenceQueryParam,
        } };
    };

    struct azurecnspeech
    {
        static constexpr const char* hostnameSuffix = ".stt.speech.azure.cn";
    };

    struct azurecntranslation
    {
        static constexpr const char* hostnameSuffix = ".s2s.speech.azure.cn";
    };

    struct azurecnspeechsynthesis
    {
        static constexpr const char* hostnameSuffix = ".tts.speech.azure.cn";
    };

    struct azurecncustomvoice
    {
        static constexpr const char* hostnameSuffix = ".voice.speech.azure.cn";
    };

    struct azurecndialog
    {
        static constexpr const char* hostnameSuffix = ".convai.speech.azure.cn";
    };

    struct translationV1
    {
        static constexpr const char* hostnameSuffix = ".s2s.speech.microsoft.com";
        static constexpr const char* path = "/speech/translation/cognitiveservices/v1";

        static constexpr const char* fromQueryParam = "from=";
        static constexpr const char* toQueryParam = "to=";
        static constexpr const char* voiceQueryParam = "voice=";
        static constexpr const char* featuresQueryParam = "features=";
        static constexpr const char* stableTranslationQueryParam = "stableTranslation=";

        static constexpr const char* requireVoice = "texttospeech";

        static constexpr std::array<const char*, 13> queryParameters{ {
            fromQueryParam,
            toQueryParam,
            voiceQueryParam,

            deploymentIdQueryParam,
            initialSilenceTimeoutQueryParam,
            endSilenceTimeoutQueryParam,
            storeAudioQueryParam,

            outputFormatQueryParam,
            wordLevelTimestampsQueryParam,
            wordLevelConfidenceQueryParam,
            profanityQueryParam,
            stableIntermediateThresholdQueryParam,

            stableTranslationQueryParam
        } };
    };

    struct dialog
    {
        static constexpr const char* hostnameSuffix = ".convai.speech.microsoft.com";

        struct resourcePath
        {
            static constexpr const char* botFramework = "";
            static constexpr const char* customCommands = "/commands";
        };

        static constexpr const char* suffix = "/api";

        struct version
        {
            static constexpr const char* botFramework = "/v3";
            static constexpr const char* customCommands = "/v1";
        };

        static constexpr const char* botIdQueryParam = "botId=";
        static constexpr const char* commandsAppIdQueryParam = "X-CommandsAppId=";
        static constexpr const char* customVoiceDeploymentIdsQueryParam = "voiceDeploymentId=";
        static constexpr const char* botStatusMessageQueryParam = "enableBotMessageStatus=";

        struct customCommands
        {
            static constexpr std::array<const char*, 7> queryParameters{ {
                commandsAppIdQueryParam,
                customVoiceDeploymentIdsQueryParam,
                initialSilenceTimeoutQueryParam,
                langQueryParam,
                outputFormatQueryParam,
                wordLevelConfidenceQueryParam,
                wordLevelTimestampsQueryParam,
            } };
        };

        struct botFramework
        {
            static constexpr std::array<const char*, 8> queryParameters{ {
                botIdQueryParam,
                botStatusMessageQueryParam,
                customVoiceDeploymentIdsQueryParam,
                initialSilenceTimeoutQueryParam,
                langQueryParam,
                outputFormatQueryParam,
                wordLevelConfidenceQueryParam,
                wordLevelTimestampsQueryParam,
            } };
        };
    };

    struct conversationTranscriber
    {
        static constexpr const char* hostnamePrefix = "transcribe.";
        static constexpr const char* hostnameSuffix = ".cts.speech.microsoft.com";
        static constexpr const char* pathPrefix = "/speech/recognition";
        static constexpr const char* pathSuffixMultiAudio = "/multiaudio";
        static constexpr const char* pathSuffixDynamic = "/dynamicaudio";

        static constexpr std::array<const char*, 5> queryParameters{ {
            langQueryParam,
            outputFormatQueryParam,
            profanityQueryParam,
            wordLevelTimestampsQueryParam,
            wordLevelConfidenceQueryParam,
        } };
    };

    struct conversationTranscriberV2
    {
        static constexpr const char* hostnameSuffix = ".stt.speech.microsoft.com";
        static constexpr const char* path = "/stt/speech/universal/v2";

        static constexpr std::array<const char*, 7> queryParameters{ {
            langQueryParam,
            deploymentIdQueryParam,
            outputFormatQueryParam,
            profanityQueryParam,
            storeAudioQueryParam,
            wordLevelTimestampsQueryParam,
            wordLevelConfidenceQueryParam,
        } };
    };

    struct speechSynthesis
    {
        static constexpr const char* deploymentIdQueryParam = "deploymentId=";

        static constexpr const char* hostnameSuffix = ".tts.speech.microsoft.com";
        static constexpr const char* path = "/tts/cognitiveservices/websocket/v1";
        static constexpr const char* hostPath = "/cognitiveservices/websocket/v1";
        static constexpr const char* restPath = "/cognitiveservices/v1";
        static constexpr const char* restCustomDomainPath = "/tts/cognitiveservices/v1";
        static constexpr const char* voicesListPath = "/tts/cognitiveservices/voices/list";
        static constexpr const char* voicesHostListPath = "/cognitiveservices/voices/list";
        static constexpr const char* customDomainPrefix = "/tts";

        static constexpr std::array<const char*, 1> queryParameters{ {
            speechSynthesis::deploymentIdQueryParam
        } };
    };

    struct customvoice
    {
        static constexpr const char* hostnameSuffix = ".voice.speech.microsoft.com";
        static constexpr const char* customDomainPrefix = "/voice";
    };
};

}}}}
