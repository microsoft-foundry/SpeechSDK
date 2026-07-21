//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// voices_info.h: Implementation declarations for CSpxVoiceInfo C++ class
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxVoiceInfo :
    public ISpxVoiceInfo,
    public ISpxVoiceInfoInit,
    public ISpxPropertyBagImpl
{
public:

    CSpxVoiceInfo();
    ~CSpxVoiceInfo() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxVoiceInfo)
        SPX_INTERFACE_MAP_ENTRY(ISpxVoiceInfoInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // --- ISpxVoiceInfo ---
    std::string GetName() override;
    std::string GetLocale() override;
    std::string GetShortName() override;
    std::string GetLocalName() override;
    std::vector<std::string> GetStyleList() override;
    std::string GetVoicePath() override;
    SynthesisVoiceType GetVoiceType() override;

    // --- ISpxVoiceInfoInit ---
    void InitVoiceInfo(std::string&& name, std::string&& locale, SynthesisVoiceType voiceType) override;
    void SetNames(std::string&& shortName, std::string&& localName) override;
    void SetStyleList(std::vector<std::string>&& styleList) override;
    void SetVoicePath(std::string&& voicePath) override;

    // --- ISpxNamedProperties (overrides)
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;  

private:

    DISABLE_COPY_AND_MOVE(CSpxVoiceInfo);

    std::string m_resultId;
    std::string m_name;
    std::string m_locale;
    std::string m_shortName;
    std::string m_localName;
    std::string m_voicePath;
    SynthesisVoiceType m_voiceType;
    std::vector<std::string> m_styleList;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
