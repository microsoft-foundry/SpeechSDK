//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_conversation_transcriptiuon_result.cpp: Public API definitions for ConversationTranscriberResult related C methods
//

#include "stdafx.h"
#include "string_utils.h"
#include "handle_helpers.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI conversation_transcription_result_get_speaker_id(SPXRESULTHANDLE hresult, char* pszSpeakerId, uint32_t cchSpeakerId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, cchSpeakerId == 0);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pszSpeakerId == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognitionResult, SPXRESULTHANDLE>();
        auto result = (*resulthandles)[hresult];

        auto conversationTranscriberResult = SpxQueryInterface<ISpxConversationTranscriptionResult>(result);

        auto strActual = conversationTranscriberResult->GetUserId();
        auto pszActual = strActual.c_str();
        PAL::strcpy(pszSpeakerId, cchSpeakerId, pszActual, strActual.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI conversation_transcription_result_get_user_id(SPXRESULTHANDLE hresult, char* pszUserId, uint32_t cchUserId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, cchUserId == 0);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pszUserId == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto resulthandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognitionResult, SPXRESULTHANDLE>();
        auto result = (*resulthandles)[hresult];

        auto conversationTranscriberResult = SpxQueryInterface<ISpxConversationTranscriptionResult>(result);
        SPX_THROW_HR_IF(SPXERR_INVALID_HANDLE, conversationTranscriberResult == nullptr);

        auto strActual = conversationTranscriberResult->GetUserId();
        auto pszActual = strActual.c_str();
        PAL::strcpy(pszUserId, cchUserId, pszActual, strActual.size(), true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
