//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// meeting.cpp: implementation declarations for meeting
//

#include "stdafx.h"
#include <sstream>
#include "meeting.h"
#include "spx_namespace.h"
#include "usp.h"
#include "http_utils.h"
#include "create_object_helpers.h"

#include "meeting_participants_mgr.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxMeeting::CSpxMeeting()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxMeeting::~CSpxMeeting()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    // meeting object may die before transcribers, after that Carbon still needs session. So, can't kill session here.
}

void CSpxMeeting::Init()
{
    SPX_DBG_TRACE_FUNCTION();
    std::shared_ptr<ISpxRecognizerSite> site = GetSite();
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, site == nullptr);

    auto genericSite = site->QueryInterface<ISpxGenericSite>();
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CONVERSATION_SITE_FAILURE, genericSite == nullptr);

    auto thread_service = SpxQueryService<ISpxThreadService>(site);
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CONVERSATION_SITE_FAILURE, thread_service == nullptr);

    m_keepSessionAlive = site;

    if (GetOr<bool>("ConversationTranscriptionInRoomAndOnline", false))
    {
        m_impl = std::make_shared<CSpxMeetingParticipantMgrImpl>(thread_service, m_keepSessionAlive);
        SPX_DBG_TRACE_INFO("Created a CSpxMeetingParticipantMgrImpl for manager participants in a meeting.");
    }
    else
    {
        m_impl = SpxCreateObjectWithSite<ISpxMeeting>("CSpxMeetingImpl", genericSite);
        SPX_DBG_TRACE_INFO("Created a CSpxMeetingImpl for the meeting service.");
    }

    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CONVERSATION_SITE_FAILURE, m_impl == nullptr);
    SetRecoMode();
}

std::shared_ptr<ISpxSession> CSpxMeeting::InternalQueryService(uint64_t serviceTypeId)
{
    return serviceTypeId == Type<ISpxSession>::Id
        ? SpxQueryService<ISpxSession>(GetSite())
        : nullptr;
}

void CSpxMeeting::SetRecoMode()
{
    // Conversation transcriber uses CONTINUOUS mode.
    SetAsDefault(PropertyId::SpeechServiceConnection_RecoMode, g_recoModeConversation);
}

void CSpxMeeting::Term()
{
    if (m_impl != nullptr)
    {
        auto objectInit = m_impl->QueryInterface<ISpxObjectInit>();
        if (objectInit != nullptr)
        {
            objectInit->Term();
        }

        m_impl = nullptr;
    }
}

void CSpxMeeting::UpdateParticipant(bool add, const std::string& userId, std::shared_ptr<ISpxParticipant> participant)
{
    ValidateImpl();
    m_impl->UpdateParticipant(add, userId, participant);
}

void CSpxMeeting::UpdateParticipant(bool add, const std::string& userId)
{
    ValidateImpl();
    m_impl->UpdateParticipant(add, userId);
}

void CSpxMeeting::UpdateParticipants(bool add, std::vector<ParticipantPtr>&& participants)
{
    ValidateImpl();
    m_impl->UpdateParticipants(add, std::move(participants));
}

void CSpxMeeting::SetMeetingId(const std::string& id)
{
    ValidateImpl();
    m_impl->SetMeetingId(id);
}

const std::string CSpxMeeting::GetMeetingId() const
{
    ValidateImpl();
    return m_impl->GetMeetingId();
}

std::string  CSpxMeeting::GetSpeechEventPayload(ISpxMeeting::MeetingState state)
{
    ValidateImpl();
    return m_impl->GetSpeechEventPayload(state);
}

void CSpxMeeting::CreateMeeting(const std::string & nickname)
{
    ValidateImpl();
    m_impl->CreateMeeting(nickname);
}

void CSpxMeeting::DeleteMeeting()
{
    ValidateImpl();
    m_impl->DeleteMeeting();
}

void CSpxMeeting::StartMeeting()
{
    ValidateImpl();
    m_impl->StartMeeting();
}

void CSpxMeeting::EndMeeting()
{
    ValidateImpl();
    m_impl->EndMeeting();
}

void CSpxMeeting::SetLockMeeting(bool lock)
{
    ValidateImpl();
    m_impl->SetLockMeeting(lock);
}

void CSpxMeeting::SetMuteAllParticipants(bool mute)
{
    ValidateImpl();
    m_impl->SetMuteAllParticipants(mute);
}

void CSpxMeeting::SetMuteParticipant(bool mute, const std::string & participantId)
{
    ValidateImpl();
    m_impl->SetMuteParticipant(mute, participantId);
}

void CSpxMeeting::SetStringValue(const char* name, const char* value)
{
    auto siteProperties = SpxQueryService<ISpxNamedProperties>(GetSite());
    siteProperties->SetStringValue(name, value);
}

void CSpxMeeting::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    auto siteProperties = SpxQueryService<ISpxNamedProperties>(GetSite());
    siteProperties->SetBinaryValue(name, value, size);
}

bool CSpxMeeting::Match(const char* name, bool fullMatch,const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const
{
    auto siteProperties = SpxQueryService<ISpxNamedProperties>(GetSite());
    return siteProperties != nullptr && siteProperties->Match(name, fullMatch, pattern, output1, outputAll, strategy, context);
}

void CSpxMeeting::ValidateImpl() const
{
    if (m_impl == nullptr)
    {
        ThrowRuntimeError("Called CSpxMeeting method without initializing the impl!");
    }
}
}}}} // Microsoft::CognitiveServices::Speech::Impl
