//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speech_recognition_model.h: Implementation declarations for CSpxSpeechRecognitionModel C++ class
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSpeechRecognitionModel :
    public ISpxSpeechRecognitionModel,
    public ISpxSpeechRecognitionModelInit,
    public ISpxPropertyBagImpl
{
public:

    CSpxSpeechRecognitionModel();
    ~CSpxSpeechRecognitionModel() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechRecognitionModel)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechRecognitionModelInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // ISpxSpeechRecognitionModel
    const std::string& GetName() override;
    const std::vector<std::string>& GetLocales() override;
    const std::string& GetPath() override;
    const std::string& GetVersion() override;

    // ISpxSpeechRecognitionModelInit
    void InitModel(std::string&& name, std::vector<std::string>&& locales, std::string&& version) override;
    void SetModelPath(std::string&& path) override;

private:

    DISABLE_COPY_AND_MOVE(CSpxSpeechRecognitionModel);

    std::string m_name;
    std::vector<std::string> m_locales;
    std::string m_path;
    std::string m_version;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
