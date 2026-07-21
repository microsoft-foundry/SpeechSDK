//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_participant.cpp: Public API declarations for conversation transcriber participant related C methods and enumerations
//

#include "stdafx.h"
#include "common.h"
#include <speechapi_c_common.h>
#include "create_object_helpers.h"
#include "handle_helpers.h"
#include "handle_table.h"
#include "platform.h"
#include "site_helpers.h"
#include "string_utils.h"
#include "spxdebug.h"
#include <assert.h>

using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace std;


SPXAPI participant_create_handle(SPXPARTICIPANTHANDLE* hparticipant, const char* userId, const char* preferred_language, const char* voice_signature)
{
    // all the arguments after userId can be optional
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, userId == nullptr || !(*userId));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hparticipant == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hparticipant = SPXHANDLE_INVALID;

        auto participant = SpxCreateObjectWithSite<ISpxParticipant>("CSpxParticipant", SpxGetRootSite());

        auto user = SpxQueryInterface<ISpxUser>(participant);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, user == nullptr);
        user->InitFromUserId(userId);

        if (preferred_language)
        {
            participant->SetPreferredLanguage(preferred_language);
        }

        if (voice_signature)
        {
            string voice{ voice_signature };
            participant->SetVoiceSignature(std::move(voice));
        }

        auto participanthandles = CSpxSharedPtrHandleTableManager::Get<ISpxParticipant, SPXPARTICIPANTHANDLE>();
        *hparticipant = participanthandles->TrackHandle(participant);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI participant_release_handle(SPXPARTICIPANTHANDLE hparticipant)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXPARTICIPANTHANDLE, ISpxParticipant>(hparticipant);
}

SPXAPI participant_set_preferred_langugage(SPXPARTICIPANTHANDLE hparticipant, const char* preferred_language)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, preferred_language == nullptr || !(*preferred_language));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto participant = SpxGetPtrFromHandle<ISpxParticipant>(hparticipant);
        if (participant && preferred_language)
        {
            participant->SetPreferredLanguage(preferred_language);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI participant_set_voice_signature(SPXPARTICIPANTHANDLE hparticipant, const char* voice_signature)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, voice_signature == nullptr || !(*voice_signature));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto participant = SpxGetPtrFromHandle<ISpxParticipant>(hparticipant);
        participant->SetVoiceSignature({ voice_signature });
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI participant_get_property_bag(SPXPARTICIPANTHANDLE hparticipant, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterfaceAlwaysNoError<SPXPARTICIPANTHANDLE, ISpxParticipant, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hparticipant, hpropbag);
}

SPXAPI participant_get_id(SPXPARTICIPANTHANDLE hparticipant, char * psz, uint32_t * pcch)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pcch == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto participant = SpxGetPtrFromHandle<ISpxParticipant>(hparticipant);
        SPX_THROW_HR_IF(SPXERR_INVALID_HANDLE, participant == nullptr);

        std::string str = participant->GetId();

        if (psz == nullptr)
        {
            // querying the required buffer length (including the trailing null)
            *pcch = static_cast<uint32_t>(str.length() + 1);
        }
        else
        {
            // copy the value with a trailing null, truncating if the buffer is too small
            size_t length = str.length() + 1;
            if (*pcch < length)
            {
                length = static_cast<size_t>(*pcch);
            }
            *pcch = static_cast<uint32_t>(snprintf(psz, length, "%s", str.c_str()));
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
