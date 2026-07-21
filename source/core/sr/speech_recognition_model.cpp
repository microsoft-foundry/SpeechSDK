//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speech_recognition_model.cpp: Implementation definitions for CSpxSpeechRecognitionModel C++ class
//

#include "stdafx.h"
#include "speech_recognition_model.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxSpeechRecognitionModel::CSpxSpeechRecognitionModel()
{
    SPX_DBG_TRACE_FUNCTION();
}

const std::string& CSpxSpeechRecognitionModel::GetName()
{
    return m_name;
}

const std::vector<std::string>& CSpxSpeechRecognitionModel::GetLocales()
{
    return m_locales;
}

const std::string& CSpxSpeechRecognitionModel::GetPath()
{
    return m_path;
}

const std::string& CSpxSpeechRecognitionModel::GetVersion()
{
    return m_version;
}

void CSpxSpeechRecognitionModel::InitModel(std::string&& name, std::vector<std::string>&& locales, std::string&& version)
{
    m_name = std::move(name);
    m_locales = std::move(locales);
    m_version = std::move(version);
}

void CSpxSpeechRecognitionModel::SetModelPath(std::string&& path)
{
    m_path = std::move(path);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
