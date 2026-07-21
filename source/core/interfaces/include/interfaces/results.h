//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <string>

#include <speechapi_cxx_enums.h>

#include <interfaces/audio.h>
#include <interfaces/base.h>
#include <interfaces/errors.h>
#include <interfaces/types.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxRecognitionResult)
{
    public:
    virtual std::string GetResultId() = 0;
    virtual std::string GetText() = 0;

    virtual ResultReason GetReason() = 0;
    virtual CancellationReason GetCancellationReason() = 0;
    virtual std::shared_ptr<ISpxErrorInformation> GetError() = 0;
    virtual NoMatchReason GetNoMatchReason() = 0;

    virtual uint64_t GetOffset() const = 0;
    virtual void SetOffset(uint64_t) = 0;
    virtual uint64_t GetDuration() const = 0;

    virtual void SetLatency(uint64_t) = 0;

    virtual std::shared_ptr<ISpxAudioDataStream> GetAudioDataStream() = 0;
};

using RecognitionResultPtr = std::shared_ptr<ISpxRecognitionResult>;

SPX_INTERFACE(ISpxRecognitionResultInit)
{
    public:
    virtual void InitIntermediateResult(const char* text, uint64_t offset, uint64_t duration, const char* phraseId) = 0;
    virtual void InitFinalResult(ResultReason reason, NoMatchReason noMatchReason, const char* text, uint64_t offset, uint64_t duration, const char* phraseId) = 0;
    virtual void InitErrorResult(const std::shared_ptr<ISpxErrorInformation>& error) = 0;
    virtual void InitEndOfStreamResult() = 0;
};

enum class TranslationStatusCode { Success, Error };

SPX_INTERFACE(ISpxTranslationRecognitionResult)
{
    public:
    virtual const std::vector<std::tuple<std::string, std::string>>& GetTranslationText() = 0;
};

SPX_INTERFACE(ISpxTranslationRecognitionResultInit)
{
    public:
    virtual void InitTranslationRecognitionResult(TranslationStatusCode status, std::vector<std::tuple<std::string, std::string>> translations, const std::wstring& failureReason) = 0;
};

SPX_INTERFACE(ISpxConversationTranscriptionResult)
{
    public:
    virtual std::string GetUserId() = 0;
    virtual std::string GetUtteranceId() = 0;
};

SPX_INTERFACE(ISpxConversationTranscriptionV2Result)
{
public:
    virtual std::string GetSpeakerId() = 0;
};

SPX_INTERFACE(ISpxConversationTranscriptionResultInit)
{
    public:
    virtual void InitConversationResult(const char* userId, const char* utteranceId = nullptr) = 0;
};

SPX_INTERFACE(ISpxConversationTranscriptionV2ResultInit)
{
public:
    virtual void InitConversationV2Result(const char* speakerId) = 0;
};

SPX_INTERFACE(ISpxMeetingTranscriptionResult)
{
public:
    virtual std::string GetUserId() = 0;
    virtual std::string GetUtteranceId() = 0;
};

SPX_INTERFACE(ISpxMeetingTranscriptionResultInit)
{
public:
    virtual void InitMeetingResult(const char* userId, const char* utteranceId = nullptr) = 0;
};

SPX_INTERFACE(ISpxVoiceInfo)
{
    public:
    virtual std::string GetName() = 0;
    virtual std::string GetLocale() = 0;
    virtual std::string GetShortName() = 0;
    virtual std::string GetLocalName() = 0;
    virtual std::vector<std::string> GetStyleList() = 0;
    virtual std::string GetVoicePath() = 0;
    virtual SynthesisVoiceType GetVoiceType() = 0;
};

SPX_INTERFACE(ISpxVoiceInfoInit)
{
    public:
    virtual void InitVoiceInfo(std::string&& name, std::string&& locale, SynthesisVoiceType voiceType) = 0;
    virtual void SetNames(std::string&& shortName, std::string&& localName) = 0;
    virtual void SetStyleList(std::vector<std::string>&& styleList) = 0;
    virtual void SetVoicePath(std::string&& voicePath) = 0;
};

SPX_INTERFACE(ISpxSynthesisVoicesResult)
{
    public:
    virtual std::shared_ptr<std::vector<std::shared_ptr<ISpxVoiceInfo>>> GetVoices() = 0;
    virtual std::string GetResultId() = 0;
    virtual ResultReason GetReason() = 0;
    virtual const std::shared_ptr<ISpxErrorInformation>& GetError() = 0;
};

SPX_INTERFACE(ISpxSynthesisVoicesResultInit)
{
    public:
    virtual void InitSuccessResult(const std::string& resultId) = 0;
    virtual void InitErrorResult(const std::shared_ptr<ISpxErrorInformation>& error, const std::string& resultId) = 0;
    virtual void AppendVoice(const std::shared_ptr<ISpxVoiceInfo>& voice) = 0;
};

} } } }
