//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>
#include <memory>

#include <interfaces/base.h>
#include <interfaces/conversation.h> // for participant until removed from conversation

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/*
// Commented out until removed from conversation.
SPX_INTERFACE(ISpxParticipant)
{
    public:
    virtual void SetPreferredLanguage(std::string&& preferredLanguage) = 0;
    virtual void SetVoiceSignature(std::string&& voiceSignature) = 0;

    virtual std::string GetPreferredLanguage() const = 0;
    virtual std::string GetVoiceSignature() const = 0;
    virtual std::string GetId() const = 0;
};
*/

using ParticipantPtr = std::shared_ptr<ISpxParticipant>;

SPX_INTERFACE(ISpxMeeting)
{
    public:

    enum class MeetingState
    {
        START,
        ONGOING,
        END
    };

    virtual void UpdateParticipant(bool add, const std::string& userId) = 0;
    virtual void UpdateParticipant(bool add, const std::string& userId, std::shared_ptr<ISpxParticipant> participant) = 0;
    virtual void UpdateParticipants(bool add, std::vector<ParticipantPtr>&& participants) = 0;
    virtual void SetMeetingId(const std::string& id) = 0;
    virtual const std::string GetMeetingId() const = 0;
    virtual std::string GetSpeechEventPayload(MeetingState state) = 0;
    virtual void EndMeeting() = 0;

    virtual void CreateMeeting(const std::string& nickname = "") = 0;
    virtual void DeleteMeeting() = 0;
    virtual void StartMeeting() = 0;
    virtual void SetLockMeeting (bool locked) = 0;
    virtual void SetMuteAllParticipants(bool mute) = 0;
    virtual void SetMuteParticipant(bool mute, const std::string& participantId) = 0;
};

}}}} // Microsoft::CognitiveServices::Speech::Impl::Meeting
