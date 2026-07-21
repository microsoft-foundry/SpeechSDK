//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "source_lang_config.h"
#include "property_id_2_name_map.h"
#include "usp.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

void CSpxSourceLanguageConfig::InitFromLanguage(const char* language)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;
    Set(PropertyId::SpeechServiceConnection_RecoLanguage, language);
}

void CSpxSourceLanguageConfig::InitFromLanguageAndEndpointId(const char* language, const char* endpointId)
{
    InitFromLanguage(language);
    Set(PropertyId::SpeechServiceConnection_EndpointId, endpointId);
}

std::string CSpxSourceLanguageConfig::GetLanguage()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !m_init);
    return GetOr(PropertyId::SpeechServiceConnection_RecoLanguage, "");
}

std::string CSpxSourceLanguageConfig::GetEndpointId()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !m_init);
    return GetOr(PropertyId::SpeechServiceConnection_EndpointId, "");
}
} } } } // Microsoft::CognitiveServices::Speech::Impl
