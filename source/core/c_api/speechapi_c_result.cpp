//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_result.cpp: Public API definitions for Result related C methods
//

#include "stdafx.h"
#include "common.h"
#include "string_utils.h"
#include "handle_table.h"
#include "handle_helpers.h"
#include "result_helpers.h"

#include "ispxinterfaces.h" // for SpxQueryInterface

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

static_assert((int)ResultReason_NoMatch == (int)ResultReason::NoMatch, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_Canceled == (int)ResultReason::Canceled, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_RecognizingSpeech == (int)ResultReason::RecognizingSpeech, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_RecognizedSpeech == (int)ResultReason::RecognizedSpeech, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_TranslatingSpeech == (int)ResultReason::TranslatingSpeech, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_TranslatedSpeech == (int)ResultReason::TranslatedSpeech, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_SynthesizingAudio == (int)ResultReason::SynthesizingAudio, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_SynthesizingAudioComplete == (int)ResultReason::SynthesizingAudioCompleted, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_RecognizingKeyword == (int)ResultReason::RecognizingKeyword, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_RecognizedKeyword == (int)ResultReason::RecognizedKeyword, "ResultReason_* enum values == ResultReason::* enum values");
static_assert((int)ResultReason_SynthesizingAudioStart == (int)ResultReason::SynthesizingAudioStarted, "ResultReason_* enum values == ResultReason::* enum values");

static_assert((int)CancellationReason_Error == (int)CancellationReason::Error, "CancellationReason_* enum values == CancellationReason::* enum values");
static_assert((int)CancellationReason_EndOfStream == (int)CancellationReason::EndOfStream, "CancellationReason_* enum values == CancellationReason::* enum values");

static_assert((int)CancellationErrorCode_NoError == (int)CancellationErrorCode::NoError, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_AuthenticationFailure == (int)CancellationErrorCode::AuthenticationFailure, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_BadRequest == (int)CancellationErrorCode::BadRequest, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_TooManyRequests == (int)CancellationErrorCode::TooManyRequests, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_Forbidden == (int)CancellationErrorCode::Forbidden, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_ConnectionFailure == (int)CancellationErrorCode::ConnectionFailure, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_ServiceTimeout == (int)CancellationErrorCode::ServiceTimeout, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_ServiceError == (int)CancellationErrorCode::ServiceError, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_ServiceUnavailable == (int)CancellationErrorCode::ServiceUnavailable, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");
static_assert((int)CancellationErrorCode_RuntimeError == (int)CancellationErrorCode::RuntimeError, "CancellationErrorCode_* enum values == CancellationErrorCode::* enum values");

static_assert((int)NoMatchReason_NotRecognized == (int)NoMatchReason::NotRecognized, "NoMatchReason_* enum values == NoMatchReason::* enum values");
static_assert((int)NoMatchReason_InitialSilenceTimeout == (int)NoMatchReason::InitialSilenceTimeout, "NoMatchReason_* enum values == NoMatchReason::* enum values");
static_assert((int)NoMatchReason_InitialBabbleTimeout == (int)NoMatchReason::InitialBabbleTimeout, "NoMatchReason_* enum values == NoMatchReason::* enum values");
static_assert((int)NoMatchReason_KeywordNotRecognized == (int)NoMatchReason::KeywordNotRecognized, "NoMatchReason_* enum values == NoMatchReason::* enum values");
static_assert((int)NoMatchReason_EndSilenceTimeout == (int)NoMatchReason::EndSilenceTimeout, "NoMatchReason_* enum values == NoMatchReason::* enum values");

static_assert((int)SynthesisVoiceType_OnlineNeural == (int)SynthesisVoiceType::OnlineNeural, "SynthesisVoiceType_* enum values == SynthesisVoiceType::* enum values");
static_assert((int)SynthesisVoiceType_OnlineStandard == (int)SynthesisVoiceType::OnlineStandard, "SynthesisVoiceType_* enum values == SynthesisVoice::* enum values");
static_assert((int)SynthesisVoiceType_OfflineNeural == (int)SynthesisVoiceType::OfflineNeural, "SynthesisVoiceType_* enum values == SynthesisVoiceType::* enum values");
static_assert((int)SynthesisVoiceType_OnlineStandard == (int)SynthesisVoiceType::OnlineStandard, "SynthesisVoiceType_* enum values == SynthesisVoiceType::* enum values");

SPXAPI result_get_result_id(SPXRESULTHANDLE hresult, char* pszResultId, uint32_t cchResultId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, cchResultId == 0);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pszResultId == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);

        auto strActual = result->GetResultId();
        auto pszActual = strActual.c_str();
        PAL::strcpy(pszResultId, cchResultId, pszActual, strActual.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_reason(SPXRESULTHANDLE hresult, Result_Reason* reason)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, reason == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        *reason = static_cast<Result_Reason>(result->GetReason());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_reason_canceled(SPXRESULTHANDLE hresult, Result_CancellationReason* reason)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, reason == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        *reason = static_cast<Result_CancellationReason>(result->GetCancellationReason());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_canceled_error_code(SPXRESULTHANDLE hresult, Result_CancellationErrorCode* errorCode)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, errorCode == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        auto error = result->GetError();
        auto resultErrorCode = error != nullptr ? error->GetCancellationCode() : CancellationErrorCode::NoError;
        *errorCode = static_cast<Result_CancellationErrorCode>(resultErrorCode);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_no_match_reason(SPXRESULTHANDLE hresult, Result_NoMatchReason* reason)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, reason == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        *reason = static_cast<Result_NoMatchReason>(result->GetNoMatchReason());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_text(SPXRESULTHANDLE hresult, char* pszText, uint32_t cchText)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, cchText == 0);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pszText == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);

        auto strActual = result->GetText();
        auto pszActual = strActual.c_str();
        PAL::strcpy(pszText, cchText, pszActual, strActual.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_offset(SPXRESULTHANDLE hresult, uint64_t* offset)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, offset == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        *offset = result->GetOffset();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_duration(SPXRESULTHANDLE hresult, uint64_t* duration)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, duration == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        *duration = result->GetDuration();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_commit_token(SPXRESULTHANDLE hresult, uint32_t* commitToken)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, commitToken == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        *commitToken = result->GetCommitToken();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_channel(SPXRESULTHANDLE hresult, uint32_t* channel)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, channel == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxRecognitionResult>(hresult);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(result);
        auto jsonResult = properties->GetStringValue(GetPropertyName(PropertyId::SpeechServiceResponse_JsonResult));
        auto jsonParsed = ajv::json::Parse(jsonResult);
        auto value = jsonParsed.ValueAt("Channel");

        if (value.IsNumber())
        {
            *channel = value.AsUint<uint32_t>();
        }
        else
        {
            *channel = 0;
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI result_get_property_bag(SPXRESULTHANDLE hresult, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXRESULTHANDLE, ISpxRecognitionResult, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hresult, hpropbag);
}

SPXAPI synth_result_get_result_id(SPXRESULTHANDLE hresult, char* resultId, uint32_t resultIdLength)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, resultId == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, resultIdLength == 0);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisResult>(hresult);

        auto resultIdStr = result->GetResultId();
        PAL::strcpy(resultId, resultIdLength, resultIdStr.c_str(), resultIdStr.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synth_result_get_reason(SPXRESULTHANDLE hresult, Result_Reason* reason)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, reason == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisResult>(hresult);
        *reason = static_cast<Result_Reason>(result->GetReason());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synth_result_get_reason_canceled(SPXRESULTHANDLE hresult, Result_CancellationReason* reason)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, reason == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisResult>(hresult);
        *reason = static_cast<Result_CancellationReason>(result->GetCancellationReason());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synth_result_get_canceled_error_code(SPXRESULTHANDLE hresult, Result_CancellationErrorCode* errorCode)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, errorCode == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisResult>(hresult);
        auto error = result->GetError();
        auto resultErrorCode = error != nullptr ? error->GetCancellationCode() : CancellationErrorCode::NoError;
        *errorCode = static_cast<Result_CancellationErrorCode>(resultErrorCode);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synth_result_get_audio_length_duration(SPXRESULTHANDLE hresult, uint32_t* length, uint64_t* duration)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, length == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisResult>(hresult);
        *length = result->GetAudioLength();

        if (duration != nullptr)
        {
            *duration = result->GetAudioDuration();
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synth_result_get_audio_data(SPXRESULTHANDLE hresult, uint8_t* buffer, uint32_t bufferSize, uint32_t* filledSize)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, buffer == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, filledSize == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisResult>(hresult);
        auto audiodata = result->GetAudioData();
        if (audiodata == nullptr)
        {
            *filledSize = 0;
        }
        else
        {
            uint32_t audioSize = static_cast<uint32_t>(audiodata->size());
            *filledSize = audioSize < bufferSize ? audioSize : bufferSize;
            memcpy(buffer, audiodata->data(), *filledSize);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synth_result_get_audio_format(SPXRESULTHANDLE hresult, SPXAUDIOSTREAMFORMATHANDLE* hformat)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hformat == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto format = SpxGetPtrFromHandle<ISpxSynthesisResult>(hresult)->GetFormat();
        *hformat = CSpxSharedPtrHandleTableManager::TrackHandle<SPXWAVEFORMATEX, SPXAUDIOSTREAMFORMATHANDLE>(format);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synth_result_get_property_bag(SPXRESULTHANDLE hresult, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXRESULTHANDLE, ISpxSynthesisResult, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hresult, hpropbag);
}

SPXAPI synthesis_voices_result_get_result_id(SPXRESULTHANDLE hresult, char* resultId, uint32_t resultIdLength)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, resultId == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, resultIdLength == 0);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisVoicesResult>(hresult);

        auto resultIdStr = result->GetResultId();
        PAL::strcpy(resultId, resultIdLength, resultIdStr.c_str(), resultIdStr.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesis_voices_result_get_reason(SPXRESULTHANDLE hresult, Result_Reason* reason)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, reason == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisVoicesResult>(hresult);
        *reason = static_cast<Result_Reason>(result->GetReason());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesis_voices_result_get_voice_num(SPXRESULTHANDLE hresult, uint32_t* voiceNum)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, voiceNum == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisVoicesResult>(hresult);
        *voiceNum = static_cast<uint32_t>(result->GetVoices()->size());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesis_voices_result_get_voice_info(SPXRESULTHANDLE hresult, uint32_t index, SPXRESULTHANDLE* hVoiceInfo)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hVoiceInfo == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<ISpxSynthesisVoicesResult>(hresult);
        auto voices = result->GetVoices();
        SPX_RETURN_HR_IF(SPXERR_OUT_OF_RANGE, index > voices->size());

        auto voiceInfoHandles = CSpxSharedPtrHandleTableManager::Get<ISpxVoiceInfo, SPXRESULTHANDLE>();
        *hVoiceInfo = voiceInfoHandles->TrackHandle(voices->at(index));
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesis_voices_result_get_property_bag(SPXRESULTHANDLE hresult, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXRESULTHANDLE, ISpxSynthesisVoicesResult, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hresult, hpropbag);
}

SPXAPI voice_info_handle_release(SPXRESULTHANDLE hVoiceInfo)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXRESULTHANDLE, ISpxVoiceInfo>(hVoiceInfo);
}

SPXAPI__(const char*) voice_info_get_name(SPXRESULTHANDLE hVoiceInfo)
{
    return result_get_char_property<ISpxVoiceInfo>(hVoiceInfo, &ISpxVoiceInfo::GetName);
}

SPXAPI__(const char*) voice_info_get_locale(SPXRESULTHANDLE hVoiceInfo)
{
    return result_get_char_property<ISpxVoiceInfo>(hVoiceInfo, &ISpxVoiceInfo::GetLocale);
}

SPXAPI__(const char*) voice_info_get_short_name(SPXRESULTHANDLE hVoiceInfo)
{
    return result_get_char_property<ISpxVoiceInfo>(hVoiceInfo, &ISpxVoiceInfo::GetShortName);
}

SPXAPI__(const char*) voice_info_get_local_name(SPXRESULTHANDLE hVoiceInfo)
{
    return result_get_char_property<ISpxVoiceInfo>(hVoiceInfo, &ISpxVoiceInfo::GetLocalName);
}

SPXAPI__(const char*) voice_info_get_style_list(SPXRESULTHANDLE hVoiceInfo)
{
    char* list = nullptr;

    if (hVoiceInfo == nullptr)
    {
        return list;
    }

    SPXAPI_INIT_HR_TRY(hr)
        {
            auto result = SpxGetPtrFromHandle<ISpxVoiceInfo>(hVoiceInfo);
            const auto tempValue = PAL::Join(result->GetStyleList(), "|");
            const auto size = tempValue.size() + 1;
            list = new char[size];
            PAL::strcpy(list, size, tempValue.c_str(), size, true);
        }
    SPXAPI_CATCH_AND_RETURN(hr, list);
}

SPXAPI__(const char*) voice_info_get_voice_path(SPXRESULTHANDLE hVoiceInfo)
{
    return result_get_char_property<ISpxVoiceInfo>(hVoiceInfo, &ISpxVoiceInfo::GetVoicePath);
}

SPXAPI voice_info_get_voice_type(SPXRESULTHANDLE hVoiceInfo, Synthesis_VoiceType* voiceType)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, voiceType == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto voice = SpxGetPtrFromHandle<ISpxVoiceInfo>(hVoiceInfo);
        *voiceType = static_cast<Synthesis_VoiceType>(voice->GetVoiceType());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI voice_info_get_property_bag(SPXRESULTHANDLE hVoiceInfo, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXRESULTHANDLE, ISpxVoiceInfo, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hVoiceInfo, hpropbag);
}
