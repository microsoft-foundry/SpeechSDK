//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_synthesizer.cpp: Public API definitions for Synthesizer related C methods

#include "stdafx.h"
#include "common.h"
#include "service_helpers.h"
#include "event_helpers.h"
#include "handle_helpers.h"
#include "platform.h"
#include "string_utils.h"
#include "async_helpers.h"
#include "result_helpers.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace Microsoft::CognitiveServices::Speech;

static_assert((int)SpeechSynthesisOutputFormat_Raw8Khz8BitMonoMULaw == (int)SpeechSynthesisOutputFormat::Raw8Khz8BitMonoMULaw, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff16Khz16KbpsMonoSiren == (int)SpeechSynthesisOutputFormat::Riff16Khz16KbpsMonoSiren, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio16Khz16KbpsMonoSiren == (int)SpeechSynthesisOutputFormat::Audio16Khz16KbpsMonoSiren, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio16Khz32KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio16Khz32KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio16Khz128KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio16Khz128KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio16Khz64KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio16Khz64KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio24Khz48KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio24Khz48KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio24Khz96KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio24Khz96KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio24Khz160KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio24Khz160KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw16Khz16BitMonoTrueSilk == (int)SpeechSynthesisOutputFormat::Raw16Khz16BitMonoTrueSilk, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff16Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Riff16Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff8Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Riff8Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff24Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Riff24Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff8Khz8BitMonoMULaw == (int)SpeechSynthesisOutputFormat::Riff8Khz8BitMonoMULaw, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw16Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Raw16Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw24Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Raw24Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw8Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Raw8Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Ogg16khz16BitMonoOpus == (int)SpeechSynthesisOutputFormat::Ogg16Khz16BitMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Ogg24Khz16BitMonoOpus == (int)SpeechSynthesisOutputFormat::Ogg24Khz16BitMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw48Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Raw48Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff48Khz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Riff48Khz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio48Khz96KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio48Khz96KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio48Khz192KBitRateMonoMp3 == (int)SpeechSynthesisOutputFormat::Audio48Khz192KBitRateMonoMp3, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Ogg48Khz16BitMonoOpus == (int)SpeechSynthesisOutputFormat::Ogg48Khz16BitMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Webm16Khz16BitMonoOpus == (int)SpeechSynthesisOutputFormat::Webm16Khz16BitMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Webm24Khz16BitMonoOpus == (int)SpeechSynthesisOutputFormat::Webm24Khz16BitMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw24Khz16BitMonoTrueSilk == (int)SpeechSynthesisOutputFormat::Raw24Khz16BitMonoTrueSilk, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Webm24Khz16Bit24KbpsMonoOpus == (int)SpeechSynthesisOutputFormat::Webm24Khz16Bit24KbpsMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio16Khz16Bit32KbpsMonoOpus == (int)SpeechSynthesisOutputFormat::Audio16Khz16Bit32KbpsMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio24Khz16Bit48KbpsMonoOpus == (int)SpeechSynthesisOutputFormat::Audio24Khz16Bit48KbpsMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Audio24Khz16Bit24KbpsMonoOpus == (int)SpeechSynthesisOutputFormat::Audio24Khz16Bit24KbpsMonoOpus, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw22050Hz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Raw22050Hz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff22050Hz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Riff22050Hz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Raw44100Hz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Raw44100Hz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_Riff44100Hz16BitMonoPcm == (int)SpeechSynthesisOutputFormat::Riff44100Hz16BitMonoPcm, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_AmrWb16000Hz == (int)SpeechSynthesisOutputFormat::AmrWb16000Hz, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");
static_assert((int)SpeechSynthesisOutputFormat_G72216Khz64Kbps == (int)SpeechSynthesisOutputFormat::G72216Khz64Kbps, "SpeechSynthesisOutputFormat_* enum values == SpeechSynthesisOutputFormat::* enum values");

static_assert((int)StreamStatus_Unknown == (int)StreamStatus::Unknown, "StreamStatus_* enum values == StreamStatus::* enum values");
static_assert((int)StreamStatus_NoData == (int)StreamStatus::NoData, "StreamStatus_* enum values == StreamStatus::* enum values");
static_assert((int)StreamStatus_PartialData == (int)StreamStatus::PartialData, "StreamStatus_* enum values == StreamStatus::* enum values");
static_assert((int)StreamStatus_AllData == (int)StreamStatus::AllData, "StreamStatus_* enum values == StreamStatus::* enum values");
static_assert((int)StreamStatus_Canceled == (int)StreamStatus::Canceled, "StreamStatus_* enum values == StreamStatus::* enum values");

SPXAPI_(bool) synthesizer_handle_is_valid(SPXSYNTHHANDLE hsynth)
{
    return CSpxApiManager::IsValid<SPXSYNTHHANDLE, ISpxSynthesizer>(hsynth);
}

SPXAPI synthesizer_handle_release(SPXSYNTHHANDLE hsynth)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthesizer = SpxGetPtrFromHandle<ISpxSynthesizer>(hsynth);
        synthesizer->SetDisposing();
        hr = CSpxApiManager::ReleaseAlwaysNoError<SPXSYNTHHANDLE, ISpxSynthesizer>(hsynth);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_(bool) synthesizer_async_handle_is_valid(SPXASYNCHANDLE hasync)
{
    return CSpxApiManager::IsValid<SPXASYNCHANDLE, CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(hasync);
}

SPXAPI synthesizer_async_handle_release(SPXASYNCHANDLE hasync)
{
    if (CSpxApiManager::IsValid<SPXASYNCHANDLE, CSpxAsyncOp<void>>(hasync))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXASYNCHANDLE, CSpxAsyncOp<void>>(hasync);
    }
    if (CSpxApiManager::IsValid<SPXASYNCHANDLE, CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(hasync))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXASYNCHANDLE, CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(hasync);
    }
    return CSpxApiManager::ReleaseAlwaysNoError<SPXASYNCHANDLE, CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>>>(hasync);
}

SPXAPI_(bool) synthesizer_result_handle_is_valid(SPXRESULTHANDLE hresult)
{
    return CSpxApiManager::IsValid<SPXRESULTHANDLE, ISpxSynthesisResult>(hresult);
}

SPXAPI synthesizer_result_handle_release(SPXRESULTHANDLE hresult)
{
    if (CSpxApiManager::IsValid<SPXRESULTHANDLE, ISpxSynthesisResult>(hresult))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXRESULTHANDLE, ISpxSynthesisResult>(hresult);
    }
    return CSpxApiManager::ReleaseAlwaysNoError<SPXRESULTHANDLE, ISpxSynthesisVoicesResult>(hresult);
}

SPXAPI_(bool) synthesizer_event_handle_is_valid(SPXEVENTHANDLE hevent)
{
    return CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxSynthesisEventArgs>(hevent) ||
        CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxWordBoundaryEventArgs>(hevent) ||
        CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxVisemeEventArgs>(hevent) ||
        CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxBookmarkEventArgs>(hevent);
}

SPXAPI synthesizer_event_handle_release(SPXEVENTHANDLE hevent)
{
    if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxSynthesisEventArgs>(hevent))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXEVENTHANDLE, ISpxSynthesisEventArgs>(hevent);
    }
    else if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxWordBoundaryEventArgs>(hevent))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXEVENTHANDLE, ISpxWordBoundaryEventArgs>(hevent);
    }
    else if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxVisemeEventArgs>(hevent))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXEVENTHANDLE, ISpxVisemeEventArgs>(hevent);
    }
    else if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxBookmarkEventArgs>(hevent))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXEVENTHANDLE, ISpxBookmarkEventArgs>(hevent);
    }
    else
    {
        return SPXERR_INVALID_HANDLE;
    }
}

SPXAPI synthesizer_get_property_bag(SPXSYNTHHANDLE hsynth, SPXPROPERTYBAGHANDLE* hpropbag)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !synthesizer_handle_is_valid(hsynth));
    return CSpxApiManager::QueryService<SPXSYNTHHANDLE, ISpxSynthesizer, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hsynth, hpropbag);
}

SPXAPI synthesizer_speak_text(SPXSYNTHHANDLE hsynth, const char* text, uint32_t textLength, SPXRESULTHANDLE* phresult)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phresult == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, text == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto result = synthesizer->Speak(std::string(text, textLength), false);
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesisResult, SPXRESULTHANDLE>();
        *phresult = resulthandles->TrackHandle(result);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_speak_ssml(SPXSYNTHHANDLE hsynth, const char* ssml, uint32_t ssmlLength, SPXRESULTHANDLE* phresult)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phresult == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, ssml == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto result = synthesizer->Speak(std::string(ssml, ssmlLength), true);
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesisResult, SPXRESULTHANDLE>();
        *phresult = resulthandles->TrackHandle(result);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_speak_request(SPXSYNTHHANDLE hsynth, SPXREQUESTHANDLE hrequest, SPXRESULTHANDLE* phresult)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phresult == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hrequest == SPXHANDLE_INVALID);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthesizer = SpxGetPtrFromHandle<ISpxSynthesizer>(hsynth);
        auto request = SpxGetPtrFromHandle<ISpxSynthesisRequest>(hrequest);

        auto result = synthesizer->Speak("", false, request);
        *phresult = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxSynthesisResult, SPXRESULTHANDLE>(result);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_speak_text_async(SPXSYNTHHANDLE hsynth, const char* text, uint32_t textLength, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phasync == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, text == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phasync = SPXHANDLE_INVALID;

        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto asyncop = synthesizer->SpeakAsync(std::string(text, textLength), false);
        auto ptr = std::make_shared<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(std::move(asyncop));

        auto asynchandles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>, SPXASYNCHANDLE>();
        *phasync = asynchandles->TrackHandle(ptr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_speak_ssml_async(SPXSYNTHHANDLE hsynth, const char* ssml, uint32_t ssmlLength, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phasync == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, ssml == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phasync = SPXHANDLE_INVALID;

        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto asyncop = synthesizer->SpeakAsync(std::string(ssml, ssmlLength), true);
        auto ptr = std::make_shared<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(std::move(asyncop));

        auto asynchandles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>, SPXASYNCHANDLE>();
        *phasync = asynchandles->TrackHandle(ptr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_speak_request_async(SPXSYNTHHANDLE hsynth, SPXREQUESTHANDLE hrequest, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phasync == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hrequest == SPXHANDLE_INVALID);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phasync = SPXHANDLE_INVALID;

        auto synthesizer = SpxGetPtrFromHandle<ISpxSynthesizer>(hsynth);
        auto request = SpxGetPtrFromHandle<ISpxSynthesisRequest>(hrequest);

        auto asyncop = synthesizer->SpeakAsync("", false, request);
        auto ptr = std::make_shared<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(std::move(asyncop));

        *phasync = CSpxSharedPtrHandleTableManager::TrackHandle<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>, SPXASYNCHANDLE>(ptr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_start_speaking_text(SPXSYNTHHANDLE hsynth, const char* text, uint32_t textLength, SPXRESULTHANDLE* phresult)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phresult == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, text == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto result = synthesizer->StartSpeaking(std::string(text, textLength), false);
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesisResult, SPXRESULTHANDLE>();
        *phresult = resulthandles->TrackHandle(result);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_start_speaking_ssml(SPXSYNTHHANDLE hsynth, const char* ssml, uint32_t ssmlLength, SPXRESULTHANDLE* phresult)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phresult == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, ssml == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto result = synthesizer->StartSpeaking(std::string(ssml, ssmlLength), true);
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesisResult, SPXRESULTHANDLE>();
        *phresult = resulthandles->TrackHandle(result);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_start_speaking_request(SPXSYNTHHANDLE hsynth, SPXREQUESTHANDLE hrequest, SPXRESULTHANDLE* phresult)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phresult == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hrequest == SPXHANDLE_INVALID);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthesizer = SpxGetPtrFromHandle<ISpxSynthesizer>(hsynth);
        auto request = SpxGetPtrFromHandle<ISpxSynthesisRequest>(hrequest);

        auto result = synthesizer->StartSpeaking("", false, request);
        *phresult = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxSynthesisResult, SPXRESULTHANDLE>(result);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_start_speaking_text_async(SPXSYNTHHANDLE hsynth, const char* text, uint32_t textLength, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phasync == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, text == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phasync = SPXHANDLE_INVALID;

        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto asyncop = synthesizer->StartSpeakingAsync(std::string(text, textLength), false);
        auto ptr = std::make_shared<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(std::move(asyncop));

        auto asynchandles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>, SPXASYNCHANDLE>();
        *phasync = asynchandles->TrackHandle(ptr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_start_speaking_ssml_async(SPXSYNTHHANDLE hsynth, const char* ssml, uint32_t ssmlLength, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phasync == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, ssml == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phasync = SPXHANDLE_INVALID;

        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto asyncop = synthesizer->StartSpeakingAsync(std::string(ssml, ssmlLength), true);
        auto ptr = std::make_shared<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>>(std::move(asyncop));

        auto asynchandles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>, SPXASYNCHANDLE>();
        *phasync = asynchandles->TrackHandle(ptr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_speak_async_wait_for(SPXASYNCHANDLE hasync, uint32_t milliseconds, SPXRESULTHANDLE* phresult)
{
    return async_operation_wait_for<ISpxSynthesisResult>(hasync, milliseconds, phresult);
}

SPXAPI synthesizer_stop_speaking(SPXSYNTHHANDLE hsynth)
{

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        synthesizer->StopSpeaking();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_stop_speaking_async(SPXSYNTHHANDLE hsynth, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phasync == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phasync = SPXHANDLE_INVALID;

        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto asyncop = synthesizer->StopSpeakingAsync();
        auto ptr = std::make_shared<CSpxAsyncOp<void>>(std::move(asyncop));

        auto asynchandles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<void>, SPXASYNCHANDLE>();
        *phasync = asynchandles->TrackHandle(ptr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_stop_speaking_async_wait_for(SPXASYNCHANDLE hasync, uint32_t milliseconds)
{
    return async_operation_wait_for(hasync, milliseconds);
}

SPXAPI synthesizer_get_voices_list(SPXSYNTHHANDLE hsynth, const char* locale, SPXRESULTHANDLE* phresult)
{
    return async_to_sync_with_result(
        hsynth,
        phresult,
        synthesizer_get_voices_list_async,
        synthesizer_get_voices_list_async_wait_for,
        synthesizer_async_handle_release,
        locale
    );
}

SPXAPI synthesizer_get_voices_list_async(SPXSYNTHHANDLE hsynth, const char* locale, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phasync == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, locale == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phasync = SPXHANDLE_INVALID;

        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hsynth];

        auto asyncop = synthesizer->GetVoicesAsync(locale);
        auto ptr = std::make_shared<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>>>(std::move(asyncop));

        auto asynchandles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>>, SPXASYNCHANDLE>();
        *phasync = asynchandles->TrackHandle(ptr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_get_voices_list_async_wait_for(SPXASYNCHANDLE hasync, uint32_t milliseconds, SPXRESULTHANDLE* phresult)
{
    return async_operation_wait_for<ISpxSynthesisVoicesResult>(hasync, milliseconds, phresult);
}

template<typename EventInterface, typename EventArgs, typename Event>
SPXHR synthesizer_set_event_callback(Event pEvent, SPXSYNTHHANDLE hSynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hSynth];

        auto pfn = [=](std::shared_ptr<EventArgs> e) {
            auto eventhandles = CSpxSharedPtrHandleTableManager::Get<EventArgs, SPXEVENTHANDLE>();
            auto hevent = eventhandles->TrackHandle(e);
            (*pCallback)(hSynth, hevent, pvContext);
        };

        auto pISpxSynthesizerEvents = SpxQueryInterface<EventInterface>(synthesizer).get();

        // Disconnect pfn first to avoid duplication
        (pISpxSynthesizerEvents->*pEvent).UnregisterAllCallbacks();

        // Add pfn
        if (pCallback != nullptr)
        {
            (pISpxSynthesizerEvents->*pEvent).RegisterCallback(pfn);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXHR synthesizer_set_session_event_callback(
    ISpxSynthesizerEvents::SessionEvent_Type ISpxSynthesizerEvents::* sessionEvent,
    SPXSYNTHHANDLE hSynth,
    PSYNTHESIS_CALLBACK_FUNC pCallback,
    void* pvContext)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        auto synthesizer = (*synthhandles)[hSynth];

        auto pfn = [=](std::shared_ptr<ISpxSessionEventArgs> e) {
            auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSessionEventArgs, SPXEVENTHANDLE>();
            auto hevent = eventhandles->TrackHandle(e);
            (*pCallback)(hSynth, hevent, pvContext);
            };

        auto pISpxSynthesizerEvents = SpxQueryInterface<ISpxSynthesizerEvents>(synthesizer).get();

        // Disconnect pfn first to avoid duplication
        (pISpxSynthesizerEvents->*sessionEvent).UnregisterAllCallbacks();

        // Add pfn
        if (pCallback != nullptr)
        {
            (pISpxSynthesizerEvents->*sessionEvent).RegisterCallback(pfn);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_started_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_event_callback<ISpxSynthesizerEvents, ISpxSynthesisEventArgs>(&ISpxSynthesizerEvents::SynthesisStarted, hsynth, pCallback, pvContext);
}

SPXAPI synthesizer_synthesizing_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_event_callback<ISpxSynthesizerEvents, ISpxSynthesisEventArgs>(&ISpxSynthesizerEvents::Synthesizing, hsynth, pCallback, pvContext);
}

SPXAPI synthesizer_completed_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_event_callback<ISpxSynthesizerEvents, ISpxSynthesisEventArgs>(&ISpxSynthesizerEvents::SynthesisCompleted, hsynth, pCallback, pvContext);
}

SPXAPI synthesizer_canceled_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_event_callback<ISpxSynthesizerEvents, ISpxSynthesisEventArgs>(&ISpxSynthesizerEvents::SynthesisCanceled, hsynth, pCallback, pvContext);
}

SPXAPI synthesizer_token_request_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_session_event_callback(&ISpxSynthesizerEvents::TokenRequested, hsynth, pCallback, pvContext);
}

SPXAPI synthesizer_word_boundary_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_event_callback<ISpxSynthesizerEvents, ISpxWordBoundaryEventArgs>(&ISpxSynthesizerEvents::WordBoundary, hsynth, pCallback, pvContext);
}

SPXAPI synthesizer_viseme_received_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_event_callback<ISpxSynthesizerEvents, ISpxVisemeEventArgs>(&ISpxSynthesizerEvents::VisemeReceived, hsynth, pCallback, pvContext);
}

SPXAPI synthesizer_bookmark_reached_set_callback(SPXSYNTHHANDLE hsynth, PSYNTHESIS_CALLBACK_FUNC pCallback, void* pvContext)
{
    return synthesizer_set_event_callback<ISpxSynthesizerEvents, ISpxBookmarkEventArgs>(&ISpxSynthesizerEvents::BookmarkReached, hsynth, pCallback, pvContext);
}

static SPXHR synthesizer_set_connection_event_callback(
    ISpxSynthesizerEvents::ConnectionEvent_Type ISpxSynthesizerEvents::*connectionEvent,
    SPXCONNECTIONHANDLE hConnection,
    CONNECTION_CALLBACK_FUNC pCallback,
    void * pvContext)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto connection = SpxGetPtrFromHandle<ISpxConnection>(hConnection);

        auto synthesizerConnection = connection->QueryInterface<ISpxSynthesizerConnection>();
        SPX_THROW_HR_IF(SPXERR_INVALID_HANDLE, synthesizerConnection == nullptr);

        auto synthesizer = synthesizerConnection->GetSynthesizer();
        
        if (connectionEvent == &ISpxSynthesizerEvents::Disconnected && synthesizer == nullptr) {
            SPX_TRACE_WARNING_IF(synthesizer == nullptr, "%s: Disconnected: synthesizer is no longer valid", __FUNCTION__);
            return hr;
        }

        SPX_THROW_HR_IF(SPXERR_INVALID_HANDLE, synthesizer == nullptr && pCallback != nullptr);
        if (synthesizer == nullptr) {
            return hr;
        }
        
        auto pfn = [=](std::shared_ptr<ISpxConnectionEventArgs> e) {
            auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxConnectionEventArgs, SPXEVENTHANDLE>();
            auto hevent = eventhandles->TrackHandle(e);
            (*pCallback)(hevent, pvContext);
        };

        auto pISpxSynthesizerEvents = SpxQueryInterface<ISpxSynthesizerEvents>(synthesizer).get();

        // Disconnect pfn first to avoid duplication
        (pISpxSynthesizerEvents->*connectionEvent).UnregisterAllCallbacks();

        // Add pfn
        if (pCallback != nullptr)
        {
            (pISpxSynthesizerEvents->*connectionEvent).RegisterCallback(pfn);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_connection_connected_set_callback(SPXCONNECTIONHANDLE hConnection, CONNECTION_CALLBACK_FUNC pCallback, void * pvContext)
{
    return synthesizer_set_connection_event_callback(&ISpxSynthesizerEvents::Connected, hConnection, pCallback, pvContext);
}

SPXAPI synthesizer_connection_disconnected_set_callback(SPXCONNECTIONHANDLE hConnection, CONNECTION_CALLBACK_FUNC pCallback, void * pvContext)
{
    return synthesizer_set_connection_event_callback(&ISpxSynthesizerEvents::Disconnected, hConnection, pCallback, pvContext);
}

SPXAPI synthesizer_synthesis_event_get_result(SPXEVENTHANDLE hevent, SPXRESULTHANDLE* phresult)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phresult == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesisEventArgs, SPXEVENTHANDLE>();
        auto synthEvent = (*eventhandles)[hevent];
        auto result = synthEvent->GetResult();

        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesisResult, SPXRESULTHANDLE>();
        *phresult = resulthandles->TrackHandle(result);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_word_boundary_event_get_values(SPXEVENTHANDLE hevent, uint64_t *pAudioOffset, uint64_t *pDuration,
                                                  uint32_t *pTextOffset, uint32_t *pWordLength, SpeechSynthesis_BoundaryType *pBoundaryType)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pAudioOffset == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pDuration == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pTextOffset == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pWordLength == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pBoundaryType == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxWordBoundaryEventArgs, SPXEVENTHANDLE>();
        auto wordBoundaryEvent = (*eventhandles)[hevent];
        *pAudioOffset = wordBoundaryEvent->GetAudioOffset();
        *pDuration = wordBoundaryEvent->GetDuration();
        *pTextOffset = wordBoundaryEvent->GetTextOffset();
        *pWordLength = wordBoundaryEvent->GetWordLength();
        *pBoundaryType = static_cast<SpeechSynthesis_BoundaryType>(wordBoundaryEvent->GetBoundaryType());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_viseme_event_get_values(SPXEVENTHANDLE hevent, uint64_t* pAudioOffset, uint32_t* pVisemeId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pAudioOffset == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pVisemeId == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxVisemeEventArgs, SPXEVENTHANDLE>();
        auto visemeEvent = (*eventhandles)[hevent];
        *pAudioOffset = visemeEvent->GetAudioOffset();
        *pVisemeId = visemeEvent->GetVisemeId();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI__(const char*) synthesizer_viseme_event_get_animation(SPXEVENTHANDLE hEvent)
{
    return result_get_char_property<ISpxVisemeEventArgs>(hEvent, &ISpxVisemeEventArgs::GetAnimation);
}

SPXAPI synthesizer_bookmark_event_get_values(SPXEVENTHANDLE hevent, uint64_t* pAudioOffset)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pAudioOffset == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxBookmarkEventArgs, SPXEVENTHANDLE>();
        auto bookmarkEvent = (*eventhandles)[hevent];
        *pAudioOffset = bookmarkEvent->GetAudioOffset();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_event_get_result_id(SPXEVENTHANDLE hEvent, char* resultId, uint32_t resultIdLength)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, resultId == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, resultIdLength == 0);
    SPXAPI_INIT_HR_TRY(hr)
    {
        std::shared_ptr<Microsoft::CognitiveServices::Speech::Impl::ISpxSpeechSynthesisMetadataEventArgs> event;
        if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxWordBoundaryEventArgs>(hEvent))
        {
            event = SpxHandleQueryInterface<ISpxWordBoundaryEventArgs, ISpxSpeechSynthesisMetadataEventArgs>(hEvent);
        }
        else if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxVisemeEventArgs>(hEvent))
        {
            event = SpxHandleQueryInterface<ISpxVisemeEventArgs, ISpxSpeechSynthesisMetadataEventArgs>(hEvent);
        }
        else if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxBookmarkEventArgs>(hEvent))
        {
            event = SpxHandleQueryInterface<ISpxBookmarkEventArgs, ISpxSpeechSynthesisMetadataEventArgs>(hEvent);
        }

        const auto resultIdStr = event->GetResultId();
        PAL::strcpy(resultId, resultIdLength, resultIdStr.c_str(), resultIdStr.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI__(const char*) synthesizer_event_get_text(SPXEVENTHANDLE hEvent)
{
    if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxWordBoundaryEventArgs>(hEvent))
    {
        return result_get_char_property<ISpxWordBoundaryEventArgs>(hEvent, &ISpxWordBoundaryEventArgs::GetText);
    }
    else if (CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxBookmarkEventArgs>(hEvent))
    {
        return result_get_char_property<ISpxBookmarkEventArgs>(hEvent, &ISpxBookmarkEventArgs::GetText);
    }
    else
    {
        return nullptr;
    }
}
