//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// embedded_speech_config.cpp: Implementation definitions for CSpxEmbeddedSpeechConfig C++ class
//

#include <algorithm>

#include "stdafx.h"

#include "create_object_helpers.h"
#include "dynamic_module.h"
#include "embedded_speech_config.h"
#include "error_info.h"
#include "file_utils.h"
#include "site_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

static const std::string c_speechRecoModelConfigFile = "lp.config";

void CSpxEmbeddedSpeechConfig::Init()
{
    auto accelerationType = GetHardwareAccelerationType();
    SetStringValue("EmbeddedSpeech_HardwareAccelerationSupport", accelerationType.c_str());
}

void CSpxEmbeddedSpeechConfig::AddSearchPath(const char* path)
{
    SPX_DBG_TRACE_VERBOSE("%s: add path \"%s\"", __FUNCTION__, path);
    m_searchPaths.push_back(path);
}

std::string CSpxEmbeddedSpeechConfig::GetSearchPathList()
{
    return PAL::Join(m_searchPaths, std::string(1, PAL::PATH_LIST_SEPARATOR).c_str());
}

uint32_t CSpxEmbeddedSpeechConfig::GetNumSpeechRecognitionModels()
{
    if (!m_speechRecoModelsInitDone)
    {
        InitSpeechRecoModels();
    }
    auto num = m_speechRecognitionModels.size();
    return num > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(num);
}

uint32_t CSpxEmbeddedSpeechConfig::GetNumSpeechTranslationModels()
{
    if (!m_speechRecoModelsInitDone)
    {
        InitSpeechRecoModels();
    }
    auto num = m_speechTranslationModels.size();
    return num > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(num);
}

std::shared_ptr<ISpxSpeechRecognitionModel> CSpxEmbeddedSpeechConfig::GetSpeechRecognitionModel(uint32_t index)
{
    if (!m_speechRecoModelsInitDone)
    {
        InitSpeechRecoModels();
    }

    if (!m_speechRecognitionModels.empty() && index < m_speechRecognitionModels.size())
    {
        return CreateSpeechRecognitionModel(m_speechRecognitionModels[index]);
    }
    else
    {
        SPX_DBG_TRACE_WARNING(
            "%s: No speech recognition models were found or the model index is invalid "
            "(number of models: %zu, requested index: %u)",
            __FUNCTION__, m_speechRecognitionModels.size(), index);

        return nullptr;
    }
}

std::shared_ptr<ISpxSpeechTranslationModel> CSpxEmbeddedSpeechConfig::GetSpeechTranslationModel(uint32_t index)
{
    if (!m_speechRecoModelsInitDone)
    {
        InitSpeechRecoModels();
    }

    if (!m_speechTranslationModels.empty() && index < m_speechTranslationModels.size())
    {
        return CreateSpeechTranslationModel(m_speechTranslationModels[index]);
    }
    else
    {
        SPX_DBG_TRACE_WARNING(
            "%s: No speech translation models were found or the model index is invalid "
            "(number of models: %zu, requested index: %u)",
            __FUNCTION__, m_speechTranslationModels.size(), index);

        return nullptr;
    }
}

std::shared_ptr<ISpxSpeechRecognitionModel> CSpxEmbeddedSpeechConfig::GetSpeechRecognitionModel(const std::string& modelName)
{
    if (!m_speechRecoModelsInitDone)
    {
        InitSpeechRecoModels();
    }

    auto result =
        find_if(m_speechRecognitionModels.begin(), m_speechRecognitionModels.end(), [&modelName](SpeechRecognitionModel model)
            {
                // Accept both full and short (= primary locale) names
                // (ref. https://aka.ms/speech/sr-languages)
                return (model.name.compare(modelName) == 0 || model.locales[0].compare(modelName) == 0);
            });

    if (result != m_speechRecognitionModels.end())
    {
        return CreateSpeechRecognitionModel(*result);
    }
    else
    {
        SPX_DBG_TRACE_WARNING(
            "%s: No speech recognition models were found or the model name is invalid "
            "(number of models: %zu, requested name: \"%s\")",
            __FUNCTION__, m_speechRecognitionModels.size(), modelName.c_str());

        return nullptr;
    }
}

std::shared_ptr<ISpxSpeechTranslationModel> CSpxEmbeddedSpeechConfig::GetSpeechTranslationModel(const std::string& modelName)
{
    if (!m_speechRecoModelsInitDone)
    {
        InitSpeechRecoModels();
    }

    // For backwards compatibility with N-to-1 models released so far.
    std::string name = modelName;
    if (modelName.compare("en-US") == 0)
    {
        name = "en";
    }
    else if (modelName.compare("zh-CN") == 0)
    {
        name = "zh-Hans";
    }

    // Find a model based on the name (as a model name or the default target language).
    auto result =
        find_if(m_speechTranslationModels.begin(), m_speechTranslationModels.end(), [&name](SpeechTranslationModel model)
            {
                return (model.name.compare(name) == 0 || model.defaultTargetLanguage.compare(name) == 0);
            });

    if (result != m_speechTranslationModels.end())
    {
        return CreateSpeechTranslationModel(*result);
    }
    else
    {
        SPX_DBG_TRACE_WARNING(
            "%s: No speech translation models were found or the model name is invalid "
            "(number of models: %zu, requested name: \"%s\")",
            __FUNCTION__, m_speechTranslationModels.size(), modelName.c_str());

        return nullptr;
    }
}

// Note: Embedded keyword recognition model is a specialized speech recognition model.
std::shared_ptr<ISpxSpeechRecognitionModel> CSpxEmbeddedSpeechConfig::GetKeywordRecognitionModel(const std::string& modelName)
{
    if (!m_speechRecoModelsInitDone)
    {
        InitSpeechRecoModels();
    }

    auto result =
        find_if(m_keywordRecognitionModels.begin(), m_keywordRecognitionModels.end(), [&modelName](SpeechRecognitionModel model)
            {
                return model.name.compare(modelName) == 0;
            });

    if (result != m_keywordRecognitionModels.end())
    {
        return CreateSpeechRecognitionModel(*result);
    }
    else
    {
        SPX_DBG_TRACE_WARNING(
            "%s: No keyword recognition models were found or the model name is invalid "
            "(number of models: %zu, requested name: \"%s\")",
            __FUNCTION__, m_keywordRecognitionModels.size(), modelName.c_str());

        return nullptr;
    }
}

std::shared_ptr<ISpxSpeechRecognitionModel> CSpxEmbeddedSpeechConfig::CreateSpeechRecognitionModel(const SpeechRecognitionModel& modelInfo)
{
    auto model = SpxCreateObjectWithSite<ISpxSpeechRecognitionModel>("CSpxSpeechRecognitionModel", SpxGetRootSite());
    auto modelInit = SpxQueryInterface<ISpxSpeechRecognitionModelInit>(model);

    modelInit->InitModel(
        std::string(modelInfo.name),
        std::vector<std::string>(modelInfo.locales),
        std::string(modelInfo.version));
    modelInit->SetModelPath(std::string(modelInfo.path));

    return model;
}

std::shared_ptr<ISpxSpeechTranslationModel> CSpxEmbeddedSpeechConfig::CreateSpeechTranslationModel(const SpeechTranslationModel& modelInfo)
{
    auto model = SpxCreateObjectWithSite<ISpxSpeechTranslationModel>("CSpxSpeechTranslationModel", SpxGetRootSite());
    auto modelInit = SpxQueryInterface<ISpxSpeechTranslationModelInit>(model);

    modelInit->InitModel(
        std::string(modelInfo.name),
        std::vector<std::string>(modelInfo.sourceLanguages),
        std::vector<std::string>(modelInfo.targetLanguages),
        std::string(modelInfo.defaultTargetLanguage),
        std::string(modelInfo.version));
    modelInit->SetModelPath(std::string(modelInfo.path));

    return model;
}

void ReadTokensFromFile(const std::string& filePath, std::vector<std::string>& tokens)
{
    std::wstring line;
#ifdef _MSC_VER
    std::wifstream fileStream(PAL::ToWString(filePath));
#else
    std::wifstream fileStream(filePath);
#endif
    if (!fileStream.is_open())
    {
        auto errNum = errno;
        auto errMsg = GetSystemErrorMsg(errNum);
        ThrowRuntimeError("Failed to open embedded speech model file '" + filePath + "' [errno " + std::to_string(errNum) + ": " + errMsg + "]");
    }

    // Tokens are listed as one per line in the file.
    while (std::getline(fileStream, line))
    {
        auto token = PAL::StringUtils::Trim(PAL::ToString(line));
        if (!token.empty())
        {
            tokens.emplace_back(token);
        }
    }
    fileStream.close();

    // Ensure the list is sorted and has no duplicates.
    std::sort(tokens.begin(), tokens.end());
    auto last = std::unique(tokens.begin(), tokens.end());
    tokens.erase(last, tokens.end());
}

void CSpxEmbeddedSpeechConfig::InitSpeechRecoModels()
{
    m_speechRecognitionModels.clear();
    m_speechTranslationModels.clear();
    m_keywordRecognitionModels.clear();

    // Find models based on their config files under specified root paths.

    std::vector<std::string> resultFiles;

    for (const auto& searchPath : m_searchPaths)
    {
        auto foundFiles = PAL::FindFiles(searchPath, { c_speechRecoModelConfigFile });
        if (!foundFiles.empty())
        {
            resultFiles.insert(resultFiles.end(), foundFiles.begin(), foundFiles.end());
        }
        else
        {
            SPX_TRACE_WARNING("%s: No model files found under path \"%s\"", __FUNCTION__, searchPath.c_str());
        }
    }

    if (!resultFiles.empty())
    {
        // Ensure the list is sorted and has no duplicates.
        std::sort(resultFiles.begin(), resultFiles.end());
        auto last = std::unique(resultFiles.begin(), resultFiles.end());
        resultFiles.erase(last, resultFiles.end());

        // Look for the name and locale(s) of each model
        for (const auto& modelFile : resultFiles)
        {
            std::string modelName;
            std::vector<std::string> modelLocales;  // in recognition models
            std::string modelVersion;
            std::string modelSourceLanguageFile;    // in translation models
            std::string modelTargetLanguageFile;    // in translation models
            std::string modelDefaultTargetLanguage; // in translation models

#ifdef _MSC_VER
            std::wifstream file(PAL::ToWString(modelFile));
#else
            std::wifstream file(modelFile);
#endif
            if (!file.is_open())
            {
                auto errNum = errno;
                auto errMsg = GetSystemErrorMsg(errNum);
                ThrowRuntimeError("Failed to open embedded speech model file '" + modelFile + "' [errno " + std::to_string(errNum) + ": " + errMsg + "]");
            }

            file.imbue(std::locale("")); // force to C locale for consistent parsing
            std::wstring line;
            while (std::getline(file, line))
            {
                auto str = PAL::ToString(line);
                auto tokens = PAL::StringUtils::Tokenize(str.c_str(), strlen(str.c_str()), "=");
                if (tokens.size() > 1)
                {
                    const auto name = PAL::StringUtils::Trim(tokens[0]);
                    const auto value = PAL::StringUtils::Trim(tokens[1]);

                    if (name == "name")
                    {
                        modelName = value;
                    }
                    else if (name == "locale")
                    {
                        // Expect that multiple supported locales are separated with ';'.
                        for (const auto& locale : PAL::split(value, ';'))
                        {
                            modelLocales.emplace_back(locale);
                        }
                    }
                    else if (name == "version")
                    {
                        modelVersion = value;
                    }
                    else if (name == "source-languages-path")
                    {
                        modelSourceLanguageFile = value;
                    }
                    else if (name == "target-languages-path")
                    {
                        modelTargetLanguageFile = value;
                    }
                    else if (name == "default-target-language")
                    {
                        modelDefaultTargetLanguage = value;
                    }
                }
            }
            file.close();

            // If the model seems legit, store info
            if (!modelName.empty() && !modelVersion.empty())
            {
                auto modelPath = modelFile.substr(0, modelFile.find_last_of("\\/"));

                if (modelName.find("Recognizer") != std::string::npos && !modelLocales.empty())
                {
                    if (modelName.find("eyword") != std::string::npos) // check for "Keyword" or "keyword"
                    {
                        SPX_DBG_TRACE_INFO("%s: Found keyword recognition model \"%s\" in %s", __FUNCTION__, modelName.c_str(), modelPath.c_str());
                        m_keywordRecognitionModels.emplace_back(modelName, modelLocales, modelPath, modelVersion);
                    }
                    else
                    {
                        SPX_DBG_TRACE_INFO("%s: Found speech recognition model \"%s\" in %s", __FUNCTION__, modelName.c_str(), modelPath.c_str());
                        m_speechRecognitionModels.emplace_back(modelName, modelLocales, modelPath, modelVersion);
                    }
                }
                else if (modelName.find("Translator") != std::string::npos && !modelTargetLanguageFile.empty())
                {
                    SPX_DBG_TRACE_INFO("%s: Found speech translation model \"%s\" in %s", __FUNCTION__, modelName.c_str(), modelPath.c_str());

                    // Read the list of supported source languages from a separate file.
                    std::vector<std::string> modelSourceLanguages;
                    modelSourceLanguageFile = modelPath + PAL::PATH_NAME_SEPARATOR + modelSourceLanguageFile;
                    SPX_DBG_TRACE_VERBOSE("%s: Source languages file \"%s\"", __FUNCTION__, modelSourceLanguageFile.c_str());

                    ReadTokensFromFile(modelSourceLanguageFile, modelSourceLanguages);

                    // Read the list of supported target languages from a separate file.
                    std::vector<std::string> modelTargetLanguages;
                    modelTargetLanguageFile = modelPath + PAL::PATH_NAME_SEPARATOR + modelTargetLanguageFile;
                    SPX_DBG_TRACE_VERBOSE("%s: Target languages file \"%s\"", __FUNCTION__, modelTargetLanguageFile.c_str());

                    ReadTokensFromFile(modelTargetLanguageFile, modelTargetLanguages);

                    m_speechTranslationModels.emplace_back(
                        modelName,
                        modelSourceLanguages,
                        modelTargetLanguages,
                        modelDefaultTargetLanguage,
                        modelPath,
                        modelVersion);
                }
                else
                {
                    SPX_DBG_TRACE_WARNING("%s: Unsupported type of model \"%s\" in %s", __FUNCTION__, modelName.c_str(), modelPath.c_str());
                }
            }
        }
    }

    SPX_DBG_TRACE_VERBOSE("%s: Number of speech recognition models: %zu", __FUNCTION__, m_speechRecognitionModels.size());
    SPX_DBG_TRACE_VERBOSE("%s: Number of speech translation models: %zu", __FUNCTION__, m_speechTranslationModels.size());
    SPX_DBG_TRACE_VERBOSE("%s: Number of keyword recognition models: %zu", __FUNCTION__, m_keywordRecognitionModels.size());

    m_speechRecoModelsInitDone = true;
}

std::string CSpxEmbeddedSpeechConfig::GetHardwareAccelerationType()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    std::string accelerationType = "none"; // default is no acceleration available

#ifdef _WIN64 // HW acceleration is only available on Windows 11 for now

    // Library that provides the interface to detect hardware acceleration
    // capabilities. At the moment it is the SR runtime but may be changed
    // to a non-SR specific library just for this purpose in the future.
    const std::string filename = "Microsoft.CognitiveServices.Speech.extension.embedded.sr.runtime.dll";

    HMODULE handle = CSpxDynamicModule::GetLibraryHandle(filename);

    if (handle == nullptr)
    {
        SPX_DBG_TRACE_VERBOSE("%s: No library to detect HW acceleration support.", __FUNCTION__);
    }
    else
    {
        // Call the library function to detect HW acceleration
        typedef int(CALLBACK* LPFNDLLFUNC1)(char*, size_t);
        LPFNDLLFUNC1 lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress(handle, "DetectHardwareAccelerationSupport");

        if (lpfnDllFunc1)
        {
            const size_t resultBufferSize = 16; // current acceleration types are shorter, but leave some room
            std::vector<char> resultBuffer(resultBufferSize);

            auto status = lpfnDllFunc1(resultBuffer.data(), resultBufferSize);
            if (status > 0)
            {
                accelerationType = resultBuffer.data();
                SPX_DBG_TRACE_VERBOSE("%s: HW accelerator found (%s).", __FUNCTION__, accelerationType.c_str());
            }
            else if (status < 0)
            {
                SPX_DBG_TRACE_ERROR("%s: Result buffer is too small.", __FUNCTION__);
            }
            else
            {
                SPX_DBG_TRACE_VERBOSE("%s: HW accelerator not found.", __FUNCTION__);
            }
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("%s: No method to detect HW acceleration support.", __FUNCTION__);
        }

        FreeLibrary(handle);
    }
#endif

    return accelerationType;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
