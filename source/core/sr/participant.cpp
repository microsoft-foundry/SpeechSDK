//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// participant.cpp: Private implementation declarations for participant
//
#include "stdafx.h"
#include "participant.h"
#include <ajv.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxParticipant::CSpxParticipant()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxParticipant::~CSpxParticipant()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxParticipant::SetPreferredLanguage(std::string&& preferredLanguage)
{
    m_preferred_language = preferredLanguage;
}

void CSpxParticipant::SetVoiceSignature(std::string&& voiceSignature)
{
    if (voiceSignature.empty())
    {
        return;
    }
    // parse throws exception when voiceSignature is ill-formated.
    auto j = ajv::json::Parse(voiceSignature);
    if (!j.IsObject())
    {
        std::string message = "Voice signature does not parse as JSON object: " + voiceSignature;
        ThrowInvalidArgumentException(message);
    }

    m_voice = voiceSignature;
}

std::string CSpxParticipant::GetPreferredLanguage() const
{
    return m_preferred_language;
}

std::string CSpxParticipant::GetVoiceSignature() const
{
    return m_voice;
}

std::string CSpxParticipant::GetId() const
{
    return m_userId;
}

std::string CSpxParticipant::GetProperty(const char* propertyName) const
{

    return this->GetStringValue(propertyName, "");
}

}}}}
