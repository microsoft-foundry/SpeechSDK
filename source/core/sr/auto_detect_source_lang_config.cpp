//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "auto_detect_source_lang_config.h"
#include "language_list_utils.h"
#include "property_id_2_name_map.h"
#include "usp.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

void CSpxAutoDetectSourceLangConfig::InitFromOpenRange()
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;
    Set(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, g_autoDetectSourceLang_OpenRange);
    Set(PropertyId::SpeechServiceConnection_RecoLanguage, "en-US");
}

void CSpxAutoDetectSourceLangConfig::InitFromLanguages(const char* languages)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;
    Set(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, languages);
}

void CSpxAutoDetectSourceLangConfig::AddSourceLanguageConfig(std::shared_ptr<ISpxSourceLanguageConfig> sourceLanguageConfig)
{
    std::string languageList = GetOr(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, "");
    auto language = sourceLanguageConfig->GetLanguage();
    CSpxLanguageListUtils::AddLangToList(language, languageList);
    Set(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, languageList.c_str());
    SPX_DBG_TRACE_INFO("%s: auto detected source languages: %s", __FUNCTION__, languageList.c_str());
    auto endpointId = sourceLanguageConfig->GetEndpointId();
    if (!endpointId.empty())
    {
        std::string endpointIdProperty = language + (string)GetPropertyName(PropertyId::SpeechServiceConnection_EndpointId);
        SetStringValue(endpointIdProperty.c_str(), endpointId.c_str());
    }
}
} } } } // Microsoft::CognitiveServices::Speech::Impl
