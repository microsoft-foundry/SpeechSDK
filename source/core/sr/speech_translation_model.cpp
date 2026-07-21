//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speech_translation_model.cpp: Implementation definitions for CSpxSpeechTranslationModel C++ class
//

#include "stdafx.h"
#include "speech_translation_model.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxSpeechTranslationModel::CSpxSpeechTranslationModel()
{
    SPX_DBG_TRACE_FUNCTION();
}

const std::string& CSpxSpeechTranslationModel::GetName()
{
    return m_name;
}

const std::vector<std::string>& CSpxSpeechTranslationModel::GetSourceLanguages()
{
    return m_sourceLanguages;
}

const std::vector<std::string>& CSpxSpeechTranslationModel::GetTargetLanguages()
{
    return m_targetLanguages;
}

const std::string& CSpxSpeechTranslationModel::GetDefaultTargetLanguage()
{
    return m_defaultTargetLanguage;
}

const std::string& CSpxSpeechTranslationModel::GetPath()
{
    return m_path;
}

const std::string& CSpxSpeechTranslationModel::GetVersion()
{
    return m_version;
}

void CSpxSpeechTranslationModel::InitModel(
    std::string&& name,
    std::vector<std::string>&& sourceLanguages,
    std::vector<std::string>&& targetLanguages,
    std::string&& defaultTargetLanguage,
    std::string&& version)
{
    m_name = std::move(name);
    m_sourceLanguages = std::move(sourceLanguages);
    m_targetLanguages = std::move(targetLanguages);
    m_defaultTargetLanguage = std::move(defaultTargetLanguage);
    m_version = std::move(version);
}

void CSpxSpeechTranslationModel::SetModelPath(std::string&& path)
{
    m_path = std::move(path);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
