//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#include <sstream>
#include "stdafx.h"
#include "translation_recognizer.h"
#include "service_helpers.h"
#include "site_helpers.h"
#include "string_utils.h"
#include "speech_translation_config.h"
#include "language_list_utils.h"
#include "property_id_2_name_map.h"
#include <ajv.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxTranslationRecognizer::CSpxTranslationRecognizer()
{
    SPX_DBG_TRACE_FUNCTION();
}

CSpxTranslationRecognizer::~CSpxTranslationRecognizer()
{
    SPX_DBG_TRACE_FUNCTION();
}

void CSpxTranslationRecognizer::Init()
{
    CSpxRecognizer::Init();
}

void CSpxTranslationRecognizer::AddTargetLanguage(const std::string& lang)
{
    auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
    if (properties == nullptr)
    {
        ThrowRuntimeError("Property bag object is null.");
    }

    auto targetLanguages = properties->GetOr(PropertyId::SpeechServiceConnection_TranslationToLanguages, "");
    CSpxLanguageListUtils::AddLangToList(lang, targetLanguages);
    properties->Set(PropertyId::SpeechServiceConnection_TranslationToLanguages, targetLanguages.c_str());
    UpdateTargetLanguages(targetLanguages);
}

void CSpxTranslationRecognizer::RemoveTargetLanguage(const std::string& lang)
{
    auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
    if (properties == nullptr)
    {
        ThrowRuntimeError("Property bag object is null.");
    }

    auto targetLanguages = properties->GetOr(PropertyId::SpeechServiceConnection_TranslationToLanguages, "");
    CSpxLanguageListUtils::RemoveLangFromList(lang, targetLanguages);

    if (targetLanguages.empty())
    {
        ThrowInvalidArgumentException("Change target languages during recognition: the target language is empty after removal.");
    }
    properties->Set(PropertyId::SpeechServiceConnection_TranslationToLanguages, targetLanguages.c_str());
    UpdateTargetLanguages(targetLanguages);
}

void CSpxTranslationRecognizer::UpdateTargetLanguages(const std::string& targetLanguages)
{
    auto languages = PAL::split(targetLanguages, CommaDelim);

    auto eventPayload = ajv::json::Build();
    eventPayload["id"] = "translation";
    eventPayload["name"] = "updateLanguage";
    eventPayload["to"] = languages;

    auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
    if (properties != nullptr)
    {
        // Build the synthesis.defaultVoices mapping for the runtime
        // updateLanguage event. For each target language, prefer a
        // per-language voice override (looked up via "<language>" +
        // "TRANSLATION-Voice", the same convention used by
        // GetPerLanguageSetting). If no per-language override is set for a
        // given language, fall back to the single
        // SpeechServiceConnection_TranslationVoice default for that
        // language. Languages with neither set are omitted from the map.
        // When the resulting map is empty, no synthesis field is added,
        // matching the JS SDK behavior when no voice is configured.
        const auto basePropertyName = std::string(GetPropertyName(PropertyId::SpeechServiceConnection_TranslationVoice));
        const auto defaultVoice = properties->GetStringValue(basePropertyName.c_str(), "");

        std::map<std::string, std::string> voices;
        for (const auto& language : languages)
        {
            auto voice = properties->GetStringValue((language + basePropertyName).c_str(), defaultVoice.c_str());
            if (!voice.empty())
            {
                voices.emplace(language, std::move(voice));
            }
        }

        if (!voices.empty())
        {
            eventPayload["synthesis"]["defaultVoices"] = voices;
        }
    }

    auto session = GetDefaultSession();
    if (session == nullptr)
    {
        ThrowRuntimeError("UpdateTargetLanguages: the session object is nullptr.");
    }
    session->SendNetworkMessage("event", eventPayload.AsJson(), false);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
