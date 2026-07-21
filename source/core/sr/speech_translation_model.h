//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speech_translation_model.h: Implementation declarations for CSpxSpeechTranslationModel C++ class
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSpeechTranslationModel :
    public ISpxSpeechTranslationModel,
    public ISpxSpeechTranslationModelInit,
    public ISpxPropertyBagImpl
{
public:

    CSpxSpeechTranslationModel();
    ~CSpxSpeechTranslationModel() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechTranslationModel)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechTranslationModelInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // ISpxSpeechTranslationModel
    const std::string& GetName() override;
    const std::vector<std::string>& GetSourceLanguages() override;
    const std::vector<std::string>& GetTargetLanguages() override;
    const std::string& GetDefaultTargetLanguage() override;
    const std::string& GetPath() override;
    const std::string& GetVersion() override;

    // ISpxSpeechTranslationModelInit
    void InitModel(
        std::string&& name,
        std::vector<std::string>&& sourceLanguages,
        std::vector<std::string>&& targetLanguages,
        std::string&& defaultTargetLanguage,
        std::string&& version) override;
    void SetModelPath(std::string&& path) override;

private:

    DISABLE_COPY_AND_MOVE(CSpxSpeechTranslationModel);

    std::string m_name;
    std::vector<std::string> m_sourceLanguages;
    std::vector<std::string> m_targetLanguages;
    std::string m_defaultTargetLanguage;
    std::string m_path;
    std::string m_version;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
