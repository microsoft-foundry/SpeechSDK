//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_meeting.cpp: Public API definitions for Meeting related C methods
//

#include "stdafx.h"
#include "common.h"
#include "event_helpers.h"
#include "handle_helpers.h"
#include "string_utils.h"
#include <cstring>

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI meeting_update_participant_by_user_id(SPXMEETINGHANDLE hmeeting, bool add, const char* userId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, userId == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->UpdateParticipant(add, userId);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_update_participant_by_user(SPXMEETINGHANDLE hmeeting, bool add, SPXUSERHANDLE huser)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, huser == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        auto user = SpxGetPtrFromHandle<ISpxUser>(huser);
        meeting->UpdateParticipant(add, user->GetId());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_update_participant(SPXMEETINGHANDLE hmeeting, bool add, SPXPARTICIPANTHANDLE hparticipant)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hparticipant == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        auto participant = SpxGetPtrFromHandle<ISpxParticipant>(hparticipant);
        auto user = SpxQueryInterface<ISpxUser>(participant);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, user == nullptr);

        meeting->UpdateParticipant(add, user->GetId(), participant);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_get_meeting_id(SPXMEETINGHANDLE hmeeting, char* id, size_t id_size)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, id == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        auto idStr = meeting->GetMeetingId();
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, idStr.length() >= id_size);
        std::memcpy(id, idStr.c_str(), idStr.length() + 1);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_end_meeting(SPXMEETINGHANDLE hmeeting)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->EndMeeting ();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_get_property_bag(SPXMEETINGHANDLE hmeeting, SPXPROPERTYBAGHANDLE* phpropbag)
{
    return CSpxApiManager::QueryInterface<SPXMEETINGHANDLE, ISpxMeeting, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hmeeting, phpropbag);
}

SPXAPI meeting_start_meeting(SPXMEETINGHANDLE hmeeting)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->StartMeeting();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_delete_meeting(SPXMEETINGHANDLE hmeeting)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->DeleteMeeting();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_lock_meeting(SPXMEETINGHANDLE hmeeting)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->SetLockMeeting(true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_unlock_meeting(SPXMEETINGHANDLE hmeeting)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->SetLockMeeting(false);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_mute_all_participants(SPXMEETINGHANDLE hmeeting)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->SetMuteAllParticipants(true);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_unmute_all_participants(SPXMEETINGHANDLE hmeeting)
{
     SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->SetMuteAllParticipants(false);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_mute_participant(SPXMEETINGHANDLE hmeeting, const char * participantId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, participantId == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->SetMuteParticipant(true, participantId);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_unmute_participant(SPXMEETINGHANDLE hmeeting, const char * participantId)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, participantId == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meeting = SpxGetPtrFromHandle<ISpxMeeting>(hmeeting);
        meeting->SetMuteParticipant(false, participantId);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI meeting_release_handle(SPXHANDLE handle)
{
    if (handle == SPXHANDLE_INVALID)
    {
        return SPX_NOERROR;
    }
    else if (CSpxApiManager::IsValid<SPXMEETINGHANDLE, ISpxMeeting>(handle))
    {
        return CSpxApiManager::ReleaseAlwaysNoError<SPXMEETINGHANDLE, ISpxMeeting>(handle);
    }
    else
    {
        return SPXERR_INVALID_HANDLE;
    }
}
