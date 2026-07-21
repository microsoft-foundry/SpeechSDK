//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// activity_session.h: Implementation declarations for CSpxActivitySession C++ class
//
#pragma once

#include <memory>

#include "util/state_machine.h"
#include "usp.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxUspRecoEngineAdapter;

class CSpxActivitySession
{
public:

    CSpxActivitySession(std::weak_ptr<Microsoft::CognitiveServices::Speech::Impl::ISpxActivityResultAdapter> wk_ptr);

    CSpxActivitySession(const CSpxActivitySession&) = delete;
    CSpxActivitySession& operator=(const CSpxActivitySession&) = delete;

    CSpxActivitySession(CSpxActivitySession&&) = default;
    CSpxActivitySession& operator=(CSpxActivitySession&&) = default;

    ~CSpxActivitySession();

    void ActivityReceived(const std::string& activityMsg);
    void AudioReceived(const USP::AudioOutputChunkMsg& audioMsg);
    void End();

private:
    enum class State : unsigned short
    {
        Start = 0,
        ActivityReceived,
        AudioReceived,
        End
    };

    using StateMachine = Impl::StateMachine<State>;

    static StateMachine InitMachine();

    std::weak_ptr<ISpxActivityResultAdapter> m_adapter;
    StateMachine m_stateMachine;

    std::string m_activity;
    std::shared_ptr<ISpxAudioOutput> m_outputStream;

    void BuildActivityMsg(const std::string& activityMsg);
    void WriteToOutputStream(const USP::AudioOutputChunkMsg& audioMsg);
    void FireActivityResult();
};

}}}} // Microsoft::CognitiveServices::Speech::Impl

