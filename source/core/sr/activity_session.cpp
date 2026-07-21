//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// activity_session.cpp: Implementation definitions for CSpxActivitySession C++ class
//
#include "stdafx.h"
#include "activity_session.h"
#include "usp_reco_engine_adapter.h"
#include "site_helpers.h"
#include "create_object_helpers.h"
#include <ajv.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

namespace ExpectedDataStreamType
{
    constexpr uint32_t TextToSpeechAudio = 1;
}

CSpxActivitySession::StateMachine CSpxActivitySession::InitMachine()
{
    return StateMachine{ {
        { State::Start, { State::ActivityReceived, State::End } },
        { State::ActivityReceived, { State::AudioReceived, State::End } },
        { State::AudioReceived, { State::AudioReceived, State::End } },
        { State::End, {} }
    }, State::Start };
}

CSpxActivitySession::CSpxActivitySession(std::weak_ptr<ISpxActivityResultAdapter> adapter) : m_adapter{ adapter }, m_stateMachine{ InitMachine() }, m_activity{}, m_outputStream{ nullptr }
{
}

CSpxActivitySession::~CSpxActivitySession()
{
    /* Transition to end to release any resources if they are held */
    End();
}

void CSpxActivitySession::BuildActivityMsg(const std::string& activityMsg)
{
    auto messageJSON = ajv::json::Parse(activityMsg);
    /* We first build the activity */
    m_activity = messageJSON["messagePayload"].AsJson();
    /* We now check if there is a stream associated */
    if (messageJSON["messageDataStreamType"].IsNumber())
    {
        auto dataStreamType = messageJSON["messageDataStreamType"].AsUint<uint32_t>();
        if (dataStreamType & ExpectedDataStreamType::TextToSpeechAudio)
        {
            m_outputStream = SpxCreateObjectWithSite<ISpxAudioOutput>("CSpxPullAudioOutputStream", SpxGetRootSite());
        }
    }
}

void CSpxActivitySession::WriteToOutputStream(const USP::AudioOutputChunkMsg& audioMsg)
{
    auto size = (uint32_t)audioMsg.audioLength;

    if (m_outputStream == nullptr)
    {
        /* If we get here, this should never be nullptr */
        SPX_THROW_HR(SPXERR_INVALID_STATE);
    }

    /* Need to update Write to take const T* */
    m_outputStream->Write(const_cast<uint8_t *>(audioMsg.audioBuffer), size);
}

void CSpxActivitySession::FireActivityResult()
{
    if (auto adapter = m_adapter.lock())
    {
        adapter->FireActivityResult(m_activity, m_outputStream);
    }
}

void CSpxActivitySession::ActivityReceived(const std::string& activityMsg)
{
    m_stateMachine.Transition(State::ActivityReceived)
        .AndThen([&]()
        {
            BuildActivityMsg(activityMsg);
            FireActivityResult();
            return Result<TransitionError>{};
        });
}

void CSpxActivitySession::AudioReceived(const USP::AudioOutputChunkMsg& audioMsg)
{
    m_stateMachine.Transition(State::AudioReceived)
        .AndThen([&]()
        {
            WriteToOutputStream(audioMsg);
            return Result<TransitionError>{};
        });
}

void CSpxActivitySession::End()
{
    m_stateMachine.Transition(State::End)
        .AndThen([&]()
        {
            if (m_outputStream != nullptr)
            {
                m_outputStream->Close();
            }
            return Result<TransitionError>{};
        });
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
