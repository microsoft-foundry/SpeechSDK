//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// embedded_speech_config.h: Implementation declarations for CSpxEmbeddedSpeechConfig C++ class
//

#pragma once

#include "spxcore_common.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "property_bag_impl.h"
#include "ispxinterfaces.h"
#include "speech_config.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxEmbeddedSpeechConfig : public CSpxSpeechConfig, public ISpxEmbeddedSpeechConfig
{
public:

    CSpxEmbeddedSpeechConfig() {}

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechConfig)
        SPX_INTERFACE_MAP_ENTRY(ISpxEmbeddedSpeechConfig)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
    SPX_INTERFACE_MAP_END()

    // ISpxEmbeddedSpeechConfig
    void Init() override;
    void AddSearchPath(const char* path) override;
    std::string GetSearchPathList() override;
    std::uint32_t GetNumSpeechRecognitionModels() override;
    std::shared_ptr<ISpxSpeechRecognitionModel> GetSpeechRecognitionModel(uint32_t index) override;
    std::shared_ptr<ISpxSpeechRecognitionModel> GetSpeechRecognitionModel(const std::string& name) override;
    std::uint32_t GetNumSpeechTranslationModels() override;
    std::shared_ptr<ISpxSpeechTranslationModel> GetSpeechTranslationModel(uint32_t index) override;
    std::shared_ptr<ISpxSpeechTranslationModel> GetSpeechTranslationModel(const std::string& name) override;
    std::shared_ptr<ISpxSpeechRecognitionModel> GetKeywordRecognitionModel(const std::string& name) override;

    // IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

private:

    DISABLE_COPY_AND_MOVE(CSpxEmbeddedSpeechConfig);

    struct SpeechRecoModel
    {
        SpeechRecoModel(std::string name, std::string path, std::string version) :
            name(name), path(path), version(version)
        {
        };

        std::string name;
        std::string path;
        std::string version;
    };

    struct SpeechRecognitionModel : SpeechRecoModel
    {
        SpeechRecognitionModel(std::string name, std::vector<std::string> locales, std::string path, std::string version) :
            SpeechRecoModel(name, path, version), locales(locales)
        {
        };

        std::vector<std::string> locales;
    };

    struct SpeechTranslationModel : SpeechRecoModel
    {
        SpeechTranslationModel(
            std::string name,
            std::vector<std::string> sourceLanguages,
            std::vector<std::string> targetLanguages,
            std::string defaultTargetLanguage,
            std::string path,
            std::string version) :
            SpeechRecoModel(name, path, version),
            sourceLanguages(sourceLanguages),
            targetLanguages(targetLanguages),
            defaultTargetLanguage(defaultTargetLanguage)
        {
        };

        std::vector<std::string> sourceLanguages;
        std::vector<std::string> targetLanguages;
        std::string defaultTargetLanguage;
    };

    std::shared_ptr<ISpxSpeechRecognitionModel> CreateSpeechRecognitionModel(const SpeechRecognitionModel& modelInfo);
    std::shared_ptr<ISpxSpeechTranslationModel> CreateSpeechTranslationModel(const SpeechTranslationModel& modelInfo);

    void InitSpeechRecoModels();
    std::string GetHardwareAccelerationType();

    std::vector<std::string> m_searchPaths;
    std::vector<SpeechRecognitionModel> m_speechRecognitionModels;
    std::vector<SpeechTranslationModel> m_speechTranslationModels;
    std::vector<SpeechRecognitionModel> m_keywordRecognitionModels;

    bool m_speechRecoModelsInitDone{ false };
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
