//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_meeting_transcriber_result.cpp: Public API definitions for MeetingTranscriber related C methods
//

#include "stdafx.h"
#include "string_utils.h"
#include "handle_helpers.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI meeting_transcription_result_get_user_id(SPXRESULTHANDLE hresult, char* pszUserId, uint32_t cchUserId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, cchUserId == 0);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pszUserId == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognitionResult, SPXRESULTHANDLE>();
        auto result = (*resulthandles)[hresult];

        auto meetingTranscriberResult = SpxQueryInterface<ISpxMeetingTranscriptionResult>(result);

        auto strActual = meetingTranscriberResult->GetUserId();
        auto pszActual = strActual.c_str();
        PAL::strcpy(pszUserId, cchUserId, pszActual, strActual.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_transcription_result_get_utterance_id(SPXRESULTHANDLE hresult, char* pszUtteranceId, uint32_t cchUtteranceId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, cchUtteranceId == 0);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pszUtteranceId == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognitionResult, SPXRESULTHANDLE>();
        auto result = (*resulthandles)[hresult];

        auto meetingTranscriberResult = SpxQueryInterface<ISpxMeetingTranscriptionResult>(result);

        auto strActual = meetingTranscriberResult->GetUtteranceId();
        auto pszActual = strActual.c_str();
        PAL::strcpy(pszUtteranceId, cchUtteranceId, pszActual, strActual.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
