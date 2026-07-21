//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// participants_mgr.cpp: implementation declarations for ISpxConversation.
//

#include "stdafx.h"
#include <ajv.h>
#include <sstream>
#include "spx_namespace.h"
#include "usp.h"
#include "http_utils.h"
#include "interfaces/ispx_http_request.h"
#include "interfaces/ispx_http_transport_factory.h"
#include "http_exception.h"
#include "create_object_helpers.h"
#include "meeting_participants_mgr.h"
#include "service_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


const char* const CSpxMeetingParticipantMgrImpl::m_meeting_properties[] = { "iCalUid", "callId", "organizer", "FLAC", "MTUri", "DifferentiateGuestSpeakers", "audiorecording", "Threadid", "OrganizerMri", "OrganizerTenantId", "UserToken" };

CSpxMeetingParticipantMgrImpl::CSpxMeetingParticipantMgrImpl(std::shared_ptr<ISpxThreadService> thread_service, std::shared_ptr<ISpxRecognizerSite> site_in)
    :m_action{ ActionType::NONE },
    m_threadService{ thread_service },
    m_site { site_in}
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    auto site = GetSite();

    auto properties = SpxQueryService<ISpxNamedProperties>(site);
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CONVERSATION_SITE_FAILURE, properties == nullptr);

    m_subscriptionKey = GetOr(PropertyId::SpeechServiceConnection_Key, "");
    m_endpoint = GetOr(PropertyId::SpeechServiceConnection_Endpoint, "");

    if ((bool)m_threadService == false)
    {
        ThrowRuntimeError("Thread Service has not started yet!");
    }
    if ((bool)site == false)
    {
        ThrowRuntimeError("Passed an nullptr as site to ParticipantMgrImpl.");
    }
}

CSpxMeetingParticipantMgrImpl::~CSpxMeetingParticipantMgrImpl()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    m_participants_so_far.clear();
    m_current_participants.clear();
}

void CSpxMeetingParticipantMgrImpl::UpdateParticipant(bool add, const std::string& userId, std::shared_ptr<ISpxParticipant> participant)
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxMeeting>(this);
    std::packaged_task<void()> task([this, keepAlive, userId = userId, add = add, participant = std::move(participant)]() {

        if (participant == nullptr)
        {
            ThrowInvalidArgumentException("Invalid participant pointer!");
        }
        UpdateParticipantInternal(add, userId, participant->GetPreferredLanguage(), participant->GetVoiceSignature(), participant->GetProperty("ParticipantDetailsJson"));
    });
    m_threadService->ExecuteSync(std::move(task));
}

void CSpxMeetingParticipantMgrImpl::UpdateParticipant(bool add, const std::string& userId)
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxMeeting>(this);
    std::packaged_task<void()> task([this, keepAlive, add, id=userId]() {

        UpdateParticipantInternal(add, id);
    });
    m_threadService->ExecuteSync(std::move(task));
}

void CSpxMeetingParticipantMgrImpl::UpdateParticipants(bool add, std::vector<ParticipantPtr>&& participants)
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxMeeting>(this);
    std::packaged_task<void()> task([this, keepAlive, add, participants]() mutable {
        UpdateParticipantsInternal(add, std::move(participants));
    });
    m_threadService->ExecuteSync(std::move(task));
}

void CSpxMeetingParticipantMgrImpl::SetMeetingId(const std::string& id)
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxMeeting>(this);
    //capture conversation_id by reference and others by value.
    std::packaged_task<void()> task([keepAlive = keepAlive, &meeting_id = m_meeting_id, id = id]() {

        meeting_id = id;
    });
    m_threadService->ExecuteSync(std::move(task));
}

const std::string CSpxMeetingParticipantMgrImpl::GetMeetingId() const
{
    auto keepAlive = ISpxInterfaceBase::shared_from_this();
    std::string id;
    std::packaged_task<void()> task([keepAlive = keepAlive, this, &id]() {
        id = m_meeting_id;
        SPX_DBG_TRACE_INFO("id inside task is %s", id.c_str());
    });
    m_threadService->ExecuteSync(std::move(task));

    return id;
}

void CSpxMeetingParticipantMgrImpl::GetQueryParams()
{
    // calendar id never changes during a meeting
    m_calendar_uid_value = GetStringValue(m_calendar_uid_key, "");

    // call id can change during a conversation.
    m_call_id_value = GetStringValue(m_call_id_key, "");
    SPX_DBG_TRACE_INFO("Retrieved calendar id as %s and call id as %s", m_calendar_uid_value.c_str(), m_call_id_value.c_str());
}


void CSpxMeetingParticipantMgrImpl::EndMeeting()
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxMeeting>(this);
    std::packaged_task<void()> task([keepAlive, this]()
    {
        SPX_TRACE_INFO("Going to send a HTTP DELETE request.");
        try
        {
            GetQueryParams();

            // the host name and path are parsed from the endpoint. The path has /meetings appended to it.
            HttpEndpointInfo endpoint;
            endpoint.EndpointUrl(m_endpoint);
            endpoint.Path("/" + endpoint.Path() + "/meetings");

            HttpAddHeaders(endpoint);
            HttpAddQueryParams(endpoint);

            HttpUtils::ParseProxyConfig(this, endpoint);
            HttpUtils::ParseSSLConfig(this, endpoint);

            auto site = m_site.lock();
            SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, nullptr == site);

            auto networkFactory = SpxQueryService<ISpxHttpTransportFactory>(site);
            SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, nullptr == networkFactory);

            auto properties = SpxQueryInterface<ISpxNamedProperties>(ISpxInterfaceBase::shared_from_this());
            SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, nullptr == properties);

            auto request = networkFactory->CreateHttpRequest(properties, site->QueryInterface<ISpxGenericSite>());
            SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE, request == nullptr);

            auto response = request->SendRequest(HttpMethod::Delete, endpoint);
            // throws HttpException in the case of errors. You can also use IsSuccess(), or GetStatusCode() if you don't want exceptions
            response->EnsureSuccess();
        }
        catch (HttpException& e)
        {
            auto message = std::string("Error in send end meeting request. Details: ") + e.what();
            ThrowRuntimeError(message);
        }
        catch (...)
        {
            throw;
        }
        SPX_TRACE_INFO("Sent a HTTP DELETE request to destroy the meeting resources in service.");
    });

    m_threadService->ExecuteSync(std::move(task));
}


std::string CSpxMeetingParticipantMgrImpl::GetSpeechEventPayload(MeetingState state)
{
    return CreateSpeechEventPayload(state);
}

void CognitiveServices::Speech::Impl::CSpxMeetingParticipantMgrImpl::CreateMeeting(const std::string &)
{
    // nothing to do here
}

void CSpxMeetingParticipantMgrImpl::StartUpdateParticipants()
{
    m_current_participants.clear();
    m_action = ActionType::NONE;
}

void CSpxMeetingParticipantMgrImpl::DoneUpdateParticipants()
{
    SendSpeechEventMessageInternal();

    if (m_action == ActionType::ADD_PARTICIPANT)
    {
        m_participants_so_far.insert(end(m_participants_so_far), begin(m_current_participants), end(m_current_participants));
    }
    else if (m_action == ActionType::REMOVE_PARTICIPANT)
    {
        for (auto& cur : m_current_participants)
        {
            m_participants_so_far.erase(std::find_if(begin(m_participants_so_far), end(m_participants_so_far), [&cur](Participant& p) { return p.id == cur.id; }));
        }
    }
}

void CSpxMeetingParticipantMgrImpl::SendSpeechEventMessageInternal()
{
    auto site = GetSite();

    auto default_session = site->GetDefaultSession();

    if (default_session)
    {
        if (!default_session->IsStreaming())
        {
            SPX_TRACE_INFO("The speech event is not being sent due to the audio session is idle");
            return;
        }

        auto payload = CreateSpeechEventPayload(MeetingState::ONGOING);
        try
        {
            default_session->SendSpeechEventMessage(std::move(payload));
        }
        catch (const std::exception& e)
        {
            SPX_TRACE_ERROR("%s", e.what());
        }
        catch (...)
        {
            SPX_TRACE_ERROR("Exception occurred in SendSpeechEventMessage");
        }
    }
}

void CSpxMeetingParticipantMgrImpl::UpdateParticipantInternal(bool add, const std::string& id, const std::string& preferred_language, const std::string& voice_signature, const std::string& details)
{
    StartUpdateParticipants();

    Participant person{ id, preferred_language, voice_signature, details };

    m_action = add ? ActionType::ADD_PARTICIPANT : ActionType::REMOVE_PARTICIPANT;

    SanityCheckParticipants(id, person);

    m_current_participants.push_back(person);
    SPX_TRACE_INFO("Added participant id='%s'", id.c_str());

    DoneUpdateParticipants();
}

// Add a group of participants in a single speech event message.
void CSpxMeetingParticipantMgrImpl::UpdateParticipantsInternal(bool add, std::vector<ParticipantPtr>&& participants)
{
    if (participants.size() == 0)
    {
        ThrowInvalidArgumentException("Nothing to do in updateparticipantsInternal.");
    }

    StartUpdateParticipants();

    m_action = add ? ActionType::ADD_PARTICIPANT : ActionType::REMOVE_PARTICIPANT;

    for (auto& participant : participants)
    {
        Participant person(participant.get());

        SanityCheckParticipants(person.id, person);

        m_current_participants.emplace_back(std::move(person));
    }

    DoneUpdateParticipants();
}

void CSpxMeetingParticipantMgrImpl::SanityCheckParticipants(const std::string& id, const Participant& person)
{
    // Can't remove if we don't have the participant in the conversation.
    if (m_action == ActionType::REMOVE_PARTICIPANT)
    {
        auto it_prev = std::find_if(begin(m_participants_so_far), end(m_participants_so_far), [&id](const Participant& p) { return p.id == id; });
        if (it_prev == end(m_participants_so_far))
        {
            std::ostringstream os;
            os << id << " has not been added before. So, it can't be removed this time!";
            ThrowInvalidArgumentException(os.str());
        }
    }

    // if we are adding/removing it the second time, erase the first one.
    auto it = std::find_if(begin(m_current_participants), end(m_current_participants), [&person](const auto& p) { return p.id == person.id; });
    if (it != end(m_current_participants))
    {
        m_current_participants.erase(it);
    }

    // check if we exceed the max limit
    auto total_participants = (int)(m_participants_so_far.size() + m_current_participants.size());
    int max_allowed_participants = GetMaxAllowedParticipants();

    if (total_participants >= max_allowed_participants)
    {
        std::ostringstream os;
        os << "The number of participants in the meeting '" << m_meeting_id << "' is " << total_participants << ". Max allowed is " << max_allowed_participants;
        ThrowInvalidArgumentException(os.str());
    }
}

int CSpxMeetingParticipantMgrImpl::GetMaxAllowedParticipants()
{
    int max_allowed_participants = -1;

    auto max_participants_set_by_client = GetStringValue("Conversation-MaximumAllowedParticipants", "");

    if (!max_participants_set_by_client.empty())
    {
        try
        {
            max_allowed_participants = std::stoi(max_participants_set_by_client, nullptr, 10);
        }
        catch (const std::invalid_argument& ia)
        {
            UNUSED(ia);
            SPX_TRACE_WARNING("Invalid maximum number of participants set. Defaulting to %d", m_max_number_of_participants);
        }
    }
    //In case of any error in parsing max allowed participants, We are defaulting to m_max_number_of_participants which is 50
    if (max_allowed_participants <= 0)
    {
        max_allowed_participants = m_max_number_of_participants;
    }
    return max_allowed_participants;
}

std::string CSpxMeetingParticipantMgrImpl::CreateSpeechEventPayload(MeetingState state)
{
    if (m_meeting_id.empty())
    {
        ThrowInvalidArgumentException("conversation id is empty! Please set a conversation id before adding participants.");
    }

    auto speech_event = ajv::json::Build();
    speech_event["id"] = "meeting";
    speech_event["meeting"]["id"] = m_meeting_id.c_str();

    std::string name;

    switch (state)
    {
    case MeetingState::START:
        name = "start";
        break;

    case MeetingState::END:
        name = "end";
        break;

    case MeetingState::ONGOING:
        if (m_action == ActionType::ADD_PARTICIPANT)
        {
            name = "join";
        }
        else if (m_action == ActionType::REMOVE_PARTICIPANT)
        {
            name = "leave";
        }
        else
        {
            ThrowLogicError("The participant is not joining or leaving a meeting! " + std::to_string((int)m_action));
        }
        break;

    default:
        ThrowLogicError("Unsupported Meeting state " + std::to_string((int)state));
    }

    speech_event["name"] = name;

    int j = 0;
    const auto& participants = state == MeetingState::START ? m_participants_so_far : m_current_participants;
    auto attendees = speech_event["meeting"]["attendees"].Parse("[]");
    for (auto iter = participants.begin(); iter != participants.end(); iter++)
    {
        const auto& participant = *iter;
        auto attendee = attendees[j++];
        attendee["id"] = participant.id;
        attendee["preferredLanguage"] = participant.preferred_language;
        attendee["voice"] = participant.voice_signature;
        attendee["details"] = participant.details;
    }

    auto constexpr cnt = sizeof(m_meeting_properties) / sizeof(char*);
    for (size_t i = 0; i < cnt; i++)
    {
        std::string p{ m_meeting_properties[i] };
        auto value = GetStringValue(m_meeting_properties[i], "");
        if (!value.empty())
        {
            // audio recording has "on" for true.
            if (p == "audiorecording")
            {
                value = value.compare("on") == 0 ? "true" : "false";
                speech_event["meeting"]["record"] = value;
            }
            else
            {
                speech_event["meeting"][p.c_str()] = value;
            }
        }
    }

    return speech_event.AsJson();
}

void CSpxMeetingParticipantMgrImpl::HttpAddHeaders(HttpEndpointInfo& endpoint)
{
    // we use either subscription key or auth token.
    bool done = false;
    if (!m_subscriptionKey.empty())
    {
        endpoint.SetHeader(SUBSCRIPTION_KEY_NAME, m_subscriptionKey);
        done = true;
    }
    else
    {
        auto authorizationToken = GetOr(PropertyId::SpeechServiceAuthorization_Token, "");
        if (authorizationToken.empty())
        {
            ThrowRuntimeError("The authorization token is empty");
        }
        endpoint.SetHeader(AUTHORIZATION_TOKEN_KEY_NAME, std::string("Bearer ") + authorizationToken);
        done = true;
    }

    if (!done)
    {
        ThrowRuntimeError("A valid subscription key or an authorization token is needed in a HTTP request.");
    }
}

void CSpxMeetingParticipantMgrImpl::HttpAddQueryParams(HttpEndpointInfo& endpoint)
{
    bool hasId = false;
    // we need to add both if we have them.
    if (!m_calendar_uid_value.empty())
    {
        endpoint.AddQueryParameter(m_calendar_uid_key, m_calendar_uid_value);
        hasId = true;
    }
    if (!m_call_id_value.empty())
    {
        endpoint.AddQueryParameter(m_call_id_key, m_call_id_value);
        hasId = true;
    }
    // it is an error, if we have none.
    if (!hasId)
    {
        ThrowRuntimeError("iCalUid or callId must be provided in sending an end meeting request.");
    }
}

std::shared_ptr<ISpxRecognizerSite> CSpxMeetingParticipantMgrImpl::GetSite() const
{
    auto site = m_site.lock();
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CONVERSATION_SITE_FAILURE, site == nullptr);
    return site;
}

std::shared_ptr<ISpxNamedProperties> CSpxMeetingParticipantMgrImpl::GetParentProperties() const
{
    return SpxQueryInterface<ISpxNamedProperties>(GetSite());
}

}}}}
