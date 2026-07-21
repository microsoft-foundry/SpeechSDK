//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "common.h"
#include "create_object_helpers.h"
#include "handle_table.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace Microsoft::CognitiveServices::Speech;

SPXAPI speech_synthesis_request_create(bool textStreamingEnabled, bool isSSML, const char* inputText, uint32_t textLength, SPXREQUESTHANDLE* hrequest)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hrequest == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hrequest = SPXHANDLE_INVALID;

        auto request = SpxCreateObjectWithSite<ISpxSynthesisRequest>("CSpxSynthesisRequest", SpxGetRootSite());

        if (textStreamingEnabled)
        {
            request->Init(SynthesisRequestInputType::TextStream, "", "");
        }
        else
        {
            auto content = std::string(inputText, textLength);
            request->Init(isSSML ? SynthesisRequestInputType::SSML : SynthesisRequestInputType::Text, content, "");
        }

        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesisRequest, SPXREQUESTHANDLE>();
        *hrequest = confighandles->TrackHandle(request);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI speech_synthesis_request_set_voice(SPXREQUESTHANDLE hrequest, const char* voice, const char* personalVoice, const char* modelName)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, voice == nullptr && personalVoice == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto request = SpxGetPtrFromHandle<ISpxSynthesisRequest>(hrequest);
        if (personalVoice != nullptr)
        {
            request->SetVoiceName(personalVoice, SynthesisRequestVoiceType::PERSONAL, modelName);
        }
        else
        {
            request->SetVoiceName(voice, SynthesisRequestVoiceType::PREBUILT, "");
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI speech_synthesis_request_send_text_piece(SPXREQUESTHANDLE hrequest, const char* text, uint32_t textLength)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, text == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto request = SpxGetPtrFromHandle<ISpxSynthesisRequest>(hrequest);
        request->SendTextPiece(std::string(text, textLength));
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI speech_synthesis_request_finish(SPXREQUESTHANDLE hrequest)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto request = SpxGetPtrFromHandle<ISpxSynthesisRequest>(hrequest);
        request->FinishInput();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI speech_synthesis_request_handle_is_valid(SPXREQUESTHANDLE hrequest)
{
    return CSpxApiManager::IsValid<SPXREQUESTHANDLE, ISpxSynthesisRequest>(hrequest);
}

SPXAPI speech_synthesis_request_release(SPXREQUESTHANDLE hrequest)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        hr = CSpxApiManager::ReleaseAlwaysNoError<SPXREQUESTHANDLE, ISpxSynthesisRequest>(hrequest);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI speech_synthesis_request_get_property_bag(SPXREQUESTHANDLE hrequest, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXREQUESTHANDLE, ISpxSynthesisRequest, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hrequest, hpropbag);
}
