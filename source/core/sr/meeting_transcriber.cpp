//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// meeting_transcriber.cpp: implementation declarations for conversation transcriber
//

#include "stdafx.h"
#include <sstream>
#include "create_object_helpers.h"
#include "meeting_transcriber.h"
#include "spx_namespace.h"
#include "usp.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxMeetingTranscriber::CSpxMeetingTranscriber()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxMeetingTranscriber::~CSpxMeetingTranscriber()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxMeetingTranscriber::JoinMeeting(std::weak_ptr <ISpxMeeting> meeting)
{
    m_meeting = meeting;
    m_has_participant = true;
}

// leave meeting should cut all the events firing., transcribing, transcribed,
// participant changed, TextMessageReceived
void CSpxMeetingTranscriber::LeaveMeeting()
{
    m_has_participant = false;
}

void CSpxMeetingTranscriber::Init()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    EnsureDefaultSession();
    CheckLogFilename();
    m_has_participant = true;
}

//todo:
void CSpxMeetingTranscriber::Term()
{
    SPX_DBG_TRACE_FUNCTION();
}

void CSpxMeetingTranscriber::SetSingleChannelAudioConfiguration()
{
    SetRecognizerParameter("speech.config", "DisableReferenceChannel", "\"True\"");
    SetRecognizerParameter("speech.config", "MicSpec", "\"1_0_0\"");
}

void CSpxMeetingTranscriber::SetAudioConfig(std::weak_ptr<ISpxAudioConfig> audio_config)
{
    m_audioInput = audio_config;
    // We have to skip the check in case the source is file because the format will not have been yet parsed at this point.
    VerifyAudioConfigurationSupport(true);
}

std::shared_ptr<ISpxAudioConfig> CSpxMeetingTranscriber::GetAudioConfig()
{
    return m_audioInput.lock();
}

void CSpxMeetingTranscriber::FireSessionStarted(const std::wstring& sessionId)
{
    auto factory = SpxQueryService<ISpxEventArgsFactory>(CheckAndGetSite());
    auto sessionEvent = factory->CreateSessionEventArgs(sessionId);
    SessionStarted.Signal(sessionEvent);
}

void CSpxMeetingTranscriber::FireSessionStopped(const std::wstring& sessionId)
{
    auto factory = SpxQueryService<ISpxEventArgsFactory>(CheckAndGetSite());
    auto sessionEvent = factory->CreateSessionEventArgs(sessionId);
    SessionStopped.Signal(sessionEvent);
}

void CSpxMeetingTranscriber::FireSpeechStartDetected(const std::wstring& sessionId, uint64_t offset)
{
    FireRecoEvent(&SpeechStartDetected, sessionId, nullptr, offset);
}

void CSpxMeetingTranscriber::FireSpeechEndDetected(const std::wstring& sessionId, uint64_t offset)
{
    FireRecoEvent(&SpeechEndDetected, sessionId, nullptr, offset);
}

void CSpxMeetingTranscriber::FireResultEvent(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    ISpxRecognizerEvents::RecoEvent_Type* event = nullptr;
    auto reason = result->GetReason();
    bool dispatch_event = true;
    switch (reason)
    {
    case ResultReason::Canceled:
        event = &Canceled;
        break;

    case ResultReason::NoMatch:
    case ResultReason::RecognizedSpeech:
    case ResultReason::RecognizedKeyword:
        if (!m_has_participant)
        {
            dispatch_event = false;
        }
        event = &FinalResult;
        SPX_DBG_TRACE_VERBOSE_IF(!event->IsConnected(), "%s: No FinalResult event signal connected!! nobody listening...", __FUNCTION__);
        break;

    case ResultReason::RecognizingSpeech:
    case ResultReason::RecognizingKeyword:
        if (!m_has_participant)
        {
            dispatch_event = false;
        }
        event = &IntermediateResult;
        break;

    default:
        // TODO: This should be changed to throw exception. But currently it causes problem in lock.
        SPX_DBG_ASSERT_WITH_MESSAGE(false, "The reason found in the result was unexpected.");
        break;
    }

    if (dispatch_event)
    {
        FireRecoEvent(event, sessionId, result);
    }
    else
    {
        SPX_TRACE_INFO("Not dispatching recognizing and recognized results due to participants left the conversation.");
    }
}

void CSpxMeetingTranscriber::FireRecoEvent(ISpxRecognizerEvents::RecoEvent_Type* event, const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result, uint64_t offset)
{
    if (event != nullptr)
    {
        if (event->IsConnected())
        {
            auto factory = SpxQueryService<ISpxEventArgsFactory>(CheckAndGetSite());
            auto recoEvent = (result != nullptr)
                ? factory->CreateRecognitionEventArgs(sessionId, result)
                : factory->CreateRecognitionEventArgs(sessionId, offset);
            event->Signal(recoEvent);
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("No listener connected to event");
        }
    }
}

CSpxAsyncOp<void> CSpxMeetingTranscriber::StartContinuousRecognitionAsync()
{
    CheckSite(GetSite().get());
    return BaseType::StartContinuousRecognitionAsync();
}

CSpxStringMap CSpxMeetingTranscriber::GetParametersFromRecognizer(std::string&& path)
{
    VerifyAudioConfigurationSupport(false);
    return CSpxRecognizer::GetParametersFromRecognizer(std::forward<std::string>(path));
}

std::shared_ptr<ISpxRecognizerSite> CSpxMeetingTranscriber::CheckAndGetSite()
{
    auto site = GetSite();
    CheckSite(site.get());
    return site;
}

void CSpxMeetingTranscriber::CheckSite(const ISpxRecognizerSite * site)
{
    if (site == nullptr)
    {
        ThrowRuntimeError("Did you forget to call JoinConversationAsync before calling StartTranscribingAsync?");
    }
}

std::shared_ptr<ISpxConnection> CSpxMeetingTranscriber::GetConnection()
{
    if (m_meeting.expired())
    {
        ThrowLogicError("MeetingTranscriber requires a Meeting to be joined before connection can be retrieved.");
    }
    return CSpxRecognizer::GetConnection();
}

void CSpxMeetingTranscriber::VerifyAudioConfigurationSupport(bool skipFromFileCheck)
{
    // Currently only mono and the 7 + reference channel formats are supported.
    std::shared_ptr<ISpxAudioConfig> audio_config_shared = m_audioInput.lock();
    if (audio_config_shared == nullptr)
    {
        // No audio config specified, so default mic will be used.
        SetSingleChannelAudioConfiguration();
    }
    else
    {
        if (!audio_config_shared->GetFileName().empty())
        {
            // This is from file.
            if (!skipFromFileCheck)
            {
                auto maybeChannelCountProperty = Get(PropertyId::AudioConfig_NumberOfChannelsForCapture);
                if (!maybeChannelCountProperty.GetOr("").empty())
                {
                    std::string channels = maybeChannelCountProperty.Get();
                    if (channels == "1")
                    {
                        SetSingleChannelAudioConfiguration();
                    }
                }
            }
        }
        else
        {
            std::shared_ptr<ISpxAudioStream> stream = audio_config_shared->GetStream();
            if (stream != nullptr)
            {
                // This is from stream
                auto formatSize = stream->GetFormat(nullptr, 0);
                auto format = SpxAllocWAVEFORMATEX(formatSize);
                stream->GetFormat(format.get(), formatSize);
                if (format->nChannels == 1)
                {
                    SetSingleChannelAudioConfiguration();
                }
            }
            else
            {
                // This is from microphone
                auto properties = SpxQueryInterface<ISpxNamedProperties>(audio_config_shared);
                if (properties != nullptr)
                {
                    auto audioProcessingOptionsJson = properties->GetOr(PropertyId::AudioConfig_AudioProcessingOptions, "");
                    if (!audioProcessingOptionsJson.empty())
                    {
                        auto audioProcessingOptions = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", SpxGetRootSite());
                        audioProcessingOptions->InitFromJson(audioProcessingOptionsJson);
                        if (audioProcessingOptions->GetMicrophoneCount() == 1)
                        {
                            SetSingleChannelAudioConfiguration();
                        }
                    }
                    else
                    {
                        // No processing options specified, we default to mono.
                        SetSingleChannelAudioConfiguration();
                    }
                }
            }
        }
    }
}

}}}} // Microsoft::CognitiveServices::Speech::Impl
