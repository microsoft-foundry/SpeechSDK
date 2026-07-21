//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "speech_translation_config.h"
#include "property_id_2_name_map.h"
#include "usp.h"
#include <sstream>
#include "language_list_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

void CSpxSpeechTranslationConfig::AddTargetLanguage(const std::string& lang)
{
    auto languageList = GetOr(PropertyId::SpeechServiceConnection_TranslationToLanguages, "");
    CSpxLanguageListUtils::AddLangToList(lang, languageList);
    Set(PropertyId::SpeechServiceConnection_TranslationToLanguages, languageList.c_str());
}

void CSpxSpeechTranslationConfig::RemoveTargetLanguage(const std::string& lang)
{
    auto languageList = GetOr(PropertyId::SpeechServiceConnection_TranslationToLanguages, "");
    CSpxLanguageListUtils::RemoveLangFromList(lang, languageList);
    Set(PropertyId::SpeechServiceConnection_TranslationToLanguages, languageList.c_str());
}

void CSpxSpeechTranslationConfig::SetCustomModelCategoryId(const std::string& categoryId)
{
    Set(PropertyId::SpeechServiceConnection_TranslationCategoryId, categoryId);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
