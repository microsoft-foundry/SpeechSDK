//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include "recognition_result.h"
#include "guid_utils.h"
#include "property_id_2_name_map.h"

using namespace std;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxRecognitionResult::CSpxRecognitionResult():
    m_resultId { PAL::CreateGuidWithoutDashesUTF8() },
    m_cancellationReason { REASON_CANCELED_NONE },
    m_error{ nullptr },
    m_noMatchReason { NO_MATCH_REASON_NONE }
{
    SPX_DBG_TRACE_FUNCTION();
}

CSpxRecognitionResult::~CSpxRecognitionResult()
{
    SPX_DBG_TRACE_FUNCTION();
}

string CSpxRecognitionResult::GetResultId()
{
    return m_resultId;
}

string CSpxRecognitionResult::GetText()
{
    return m_text;
}

ResultReason CSpxRecognitionResult::GetReason()
{
    return m_reason;
}

CancellationReason CSpxRecognitionResult::GetCancellationReason()
{
    return m_cancellationReason;
}

std::shared_ptr<ISpxErrorInformation> CSpxRecognitionResult::GetError()
{
    return m_error;
}

NoMatchReason CSpxRecognitionResult::GetNoMatchReason()
{
    return m_noMatchReason;
}

void CSpxRecognitionResult::InitIntermediateResult(const char* text, uint64_t offset, uint64_t duration, const char* phraseId)
{
    m_reason = ResultReason::RecognizingSpeech;

    m_offset = offset;
    m_duration = duration;

    m_text = text;

    // If there is a phrase Id - use it as result Id
    if(strcmp(phraseId, ""))
    {
        m_resultId = phraseId;
    }

    SPX_TRACE_VERBOSE("%s: resultId=%s", __FUNCTION__, m_resultId.c_str());
}

void CSpxRecognitionResult::InitFinalResult(
    ResultReason reason,
    NoMatchReason noMatchReason,
    const char* text,
    uint64_t offset,
    uint64_t duration,
    const char* phraseId)
{
    SPX_DBG_TRACE_FUNCTION();

    m_reason = reason;
    m_noMatchReason = noMatchReason;
    m_text = text;
    m_offset = offset;
    m_duration = duration;

    // If there is a phrase Id - use it as result Id
    if (strcmp(phraseId, ""))
    {
        m_resultId = phraseId;
    }

    SPX_TRACE_VERBOSE("%s: resultId=%s reason=%d, text='%s'", __FUNCTION__, m_resultId.c_str(), (int)m_reason, m_text.c_str());

    #ifdef _DEBUG
    std::multimap<std::string, VariantValue> outputAll;
    Match(nullptr, false, nullptr, nullptr, &outputAll, NoMatchContinueStrategy::None, nullptr);
    for (const auto& it : outputAll)
    {
        auto psz = it.second.AsString();
        SPX_TRACE_VERBOSE_IF(psz != nullptr, "  ['%s']='%s'", it.first.c_str(), psz);
    }
    #endif
}

void CSpxRecognitionResult::InitErrorResult(const std::shared_ptr<ISpxErrorInformation>& error)
{
    SPX_DBG_TRACE_FUNCTION();

    m_reason = ResultReason::Canceled;
    m_cancellationReason = error->GetCancellationReason();
    m_error = error;

    Set(PropertyId::SpeechServiceResponse_JsonErrorDetails, error->GetDetails().c_str());
}

void CSpxRecognitionResult::InitEndOfStreamResult()
{
    SPX_DBG_TRACE_FUNCTION();

    m_reason = ResultReason::Canceled;
    m_cancellationReason = CancellationReason::EndOfStream;
}

void CSpxRecognitionResult::SetLatency(uint64_t latencyInTicks)
{
    Set(PropertyId::SpeechServiceResponse_RecognitionLatencyMs, std::to_string(latencyInTicks).c_str());
}

double CSpxRecognitionResult::GetConfidence()
{
    return m_confidence;
}

string CSpxRecognitionResult::GetUserId()
{
    return m_userId;
}

string CSpxRecognitionResult::GetUtteranceId()
{
    return m_utteranceId;
}

string CSpxRecognitionResult::GetSpeakerId()
{
    return m_speakerId;
}

void CSpxRecognitionResult::InitKeywordResult(const double confidence, const uint64_t offset, const uint64_t duration, const char* keyword, ResultReason reason, std::shared_ptr<ISpxAudioDataStream> stream)
{
    SPX_DBG_TRACE_FUNCTION();

    m_reason = reason;
    m_cancellationReason = REASON_CANCELED_NONE;
    m_noMatchReason = reason == ResultReason::NoMatch ? NoMatchReason::KeywordNotRecognized : NO_MATCH_REASON_NONE;

    m_offset = offset;
    m_duration = duration;
    m_confidence = confidence;

    m_resultId = PAL::CreateGuidWithoutDashesUTF8();
    m_text = keyword;

    m_stream = stream;

    SPX_TRACE_VERBOSE("%s: resultId=%s", __FUNCTION__, m_resultId.c_str());
}

const std::vector<std::tuple<string, string>>& CSpxRecognitionResult::GetTranslationText()
{
    return m_translations;
}

void CSpxRecognitionResult::InitTranslationRecognitionResult(TranslationStatusCode status, std::vector<std::tuple<std::string, std::string>> translations, const wstring& failureReason)
{
    SPX_DBG_TRACE_FUNCTION();

    m_translations = std::move(translations);

    if (status == TranslationStatusCode::Success)
    {
        switch (m_reason)
        {
        case ResultReason::RecognizingSpeech:
            m_reason = ResultReason::TranslatingSpeech;
            break;
        case ResultReason::RecognizedSpeech:
            m_reason = ResultReason::TranslatedSpeech;
            break;
        case ResultReason::NoMatch:
            // no match is  also considered as success and passed through.
            break;
        default:
            SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
            break;
        }
    }
    else if (status == TranslationStatusCode::Error)
    {
        // Since speech recognition is successful but only translation returns an error, we do not create a recognition error event.
        // Instead, m_reason is not upgraded to TranslatingSpeech/TranslatedSpeech, but remains as RecognizingSpeech/RecognizedSpeech,
        // and the property SpeechServiceResponse_JsonErrorDetails is set with the translation error details.
        auto errorDetails = PAL::ToString(failureReason);
        SPX_TRACE_ERROR("%s: Recognition succeeded but translation has error. Error details: %s", __FUNCTION__, errorDetails.c_str());
        Set(PropertyId::SpeechServiceResponse_JsonErrorDetails, errorDetails.c_str());
    }
    else
    {
        SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
    }
}

void CSpxRecognitionResult::InitConversationResult(const char* userId, const char* utteranceId)
{
    m_userId = (userId != nullptr) ? userId : "";
    m_utteranceId = (utteranceId != nullptr) ? utteranceId : "";
}

void CSpxRecognitionResult::InitConversationV2Result(const char* speakerId)
{
    m_speakerId = (speakerId != nullptr) ? speakerId : "";
}

void CSpxRecognitionResult::InitMeetingResult(const char* userId, const char* utteranceId)
{
    m_userId = (userId != nullptr) ? userId : "";
    m_utteranceId = (utteranceId != nullptr) ? utteranceId : "";
}

const uint8_t* CSpxRecognitionResult::GetAudio() const
{
    return m_audioBuffer.data();
}

size_t CSpxRecognitionResult::GetLength() const
{
    return m_audioLength;
}

string CSpxRecognitionResult::GetRequestId() const
{
    return m_requestId;
}

void CSpxRecognitionResult::InitTranslationSynthesisResult(const uint8_t* audioData, size_t audioLength, const string& requestId)
{
    SPX_DBG_TRACE_FUNCTION();

    m_audioBuffer.assign(audioData, audioData + audioLength);
    m_audioLength = audioLength;
    m_requestId = requestId;

    m_reason = m_audioLength > 0
        ? ResultReason::SynthesizingAudio
        : ResultReason::SynthesizingAudioCompleted;
}

void CSpxRecognitionResult::SetStringValue(const char* name, const char* value)
{
    ISpxPropertyBagImpl::SetStringValue(name, value);
}

void CSpxRecognitionResult::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    ISpxPropertyBagImpl::SetBinaryValue(name, value, size);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
