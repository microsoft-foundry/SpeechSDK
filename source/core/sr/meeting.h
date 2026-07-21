//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// meeting.h: Private implementation declarations for ISpxConversation interface.
//

#pragma once
#include "recognizer.h"
#include "thread_service.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxMeeting :
    public ISpxMeeting,
    public ISpxMeetingWithImpl,
    public ISpxServiceProvider,
    public ISpxNamedProperties,
    public ISpxObjectWithSiteInitImpl<ISpxRecognizerSite>  // needs getDefaultSession
{
public:

   SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxMeeting)
        SPX_INTERFACE_MAP_ENTRY(ISpxMeetingWithImpl)
   SPX_INTERFACE_MAP_END()

    CSpxMeeting();
    virtual ~CSpxMeeting();

      // --- ISpxObjectInit
    void Init() override;
    void Term() override;

    // --- IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
        SPX_SERVICE_MAP_ENTRY_FUNC(InternalQueryService)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

    // --- ISpxConversation
    void UpdateParticipant(bool add, const std::string& userId) override;
    void UpdateParticipant(bool add, const std::string& userId, std::shared_ptr<ISpxParticipant> participant) override;
    void UpdateParticipants(bool add, std::vector<ParticipantPtr>&& participants) override;
    void SetMeetingId(const std::string& id) override;
    const std::string GetMeetingId() const override;
    std::string GetSpeechEventPayload(MeetingState state) override;

    virtual void CreateMeeting(const std::string & nickname = "") override;
    virtual void DeleteMeeting() override;
    virtual void StartMeeting() override;
    virtual void EndMeeting() override;
    virtual void SetLockMeeting(bool lock) override;
    virtual void SetMuteAllParticipants(bool mute) override;
    virtual void SetMuteParticipant(bool mute, const std::string &) override;

    // --- ISpxMeetingWithImpl
    virtual std::shared_ptr<ISpxMeeting> GetMeetingImpl() override { return m_impl; }

    // --- ISpxNamedProperties
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;
    bool Match(const char* name, bool fullMatch,const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const override;

protected:
    std::shared_ptr<ISpxSession> InternalQueryService(uint64_t serviceTypeId);

private:
    std::shared_ptr<ISpxRecognizerSite> m_keepSessionAlive;
    std::shared_ptr<ISpxMeeting> m_impl;

    void SetRecoMode();
    void ValidateImpl() const;

    DISABLE_COPY_AND_MOVE(CSpxMeeting);
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
