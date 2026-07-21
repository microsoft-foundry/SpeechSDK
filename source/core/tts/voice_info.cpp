//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// voice_info.cpp: Implementation definitions for CSpxVoiceInfo C++ class
//

#include "stdafx.h"
#include "voice_info.h"
#include "site_helpers.h"
#include "create_object_helpers.h"
#include "property_id_2_name_map.h"
#include "synthesis_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxVoiceInfo::CSpxVoiceInfo(): m_voiceType()
{
    SPX_DBG_TRACE_FUNCTION();
}

std::string CSpxVoiceInfo::GetName()
{
    return m_name;
}

std::string CSpxVoiceInfo::GetLocale()
{
    return m_locale;
}

std::string CSpxVoiceInfo::GetShortName()
{
    return m_shortName;
}

std::string CSpxVoiceInfo::GetLocalName()
{
    return m_localName;
}

std::vector<std::string> CSpxVoiceInfo::GetStyleList()
{
    return m_styleList;
}

std::string CSpxVoiceInfo::GetVoicePath()
{
    return m_voicePath;
}

SynthesisVoiceType CSpxVoiceInfo::GetVoiceType()
{
    return m_voiceType;
}

void CSpxVoiceInfo::InitVoiceInfo(std::string&& name, std::string&& locale, SynthesisVoiceType voiceType)
{
    m_name = std::move(name);
    m_locale = std::move(locale);
    m_voiceType = voiceType;
}

void CSpxVoiceInfo::SetNames(std::string&& shortName, std::string&& localName)
{
    m_shortName = std::move(shortName);
    m_localName = std::move(localName);
}

void CSpxVoiceInfo::SetStyleList(std::vector<std::string>&& styleList)
{
    m_styleList = std::move(styleList);
}

void CSpxVoiceInfo::SetVoicePath(std::string&& voicePath)
{
    m_voicePath = std::move(voicePath);
}

void CSpxVoiceInfo::SetStringValue(const char* name, const char* value)
{
    ISpxPropertyBagImpl::SetStringValue(name, value);
}

void CSpxVoiceInfo::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    ISpxPropertyBagImpl::SetBinaryValue(name, value, size);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
