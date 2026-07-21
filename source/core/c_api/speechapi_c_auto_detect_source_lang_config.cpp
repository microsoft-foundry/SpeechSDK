//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "common.h"
#include "speechapi_c_auto_detect_source_lang_config.h"
#include "create_object_helpers.h"
#include "event_helpers.h"
#include "handle_helpers.h"
#include "platform.h"
#include "site_helpers.h"
#include "string_utils.h"
#include "handle_table.h"
#include "property_id_2_name_map.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI create_auto_detect_source_lang_config_from_open_range(SPXAUTODETECTSOURCELANGCONFIGHANDLE* hAutoDetectSourceLanguageconfig)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hAutoDetectSourceLanguageconfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hAutoDetectSourceLanguageconfig = SPXHANDLE_INVALID;

        auto autoDetectSourceLangConfig = SpxCreateObjectWithSite<ISpxAutoDetectSourceLangConfig>("CSpxAutoDetectSourceLangConfig", SpxGetRootSite());
        autoDetectSourceLangConfig->InitFromOpenRange();

        auto autoDetectSourceLangConfigs = CSpxSharedPtrHandleTableManager::Get<ISpxAutoDetectSourceLangConfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE>();
        *hAutoDetectSourceLanguageconfig = autoDetectSourceLangConfigs->TrackHandle(autoDetectSourceLangConfig);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI create_auto_detect_source_lang_config_from_languages(SPXAUTODETECTSOURCELANGCONFIGHANDLE* hAutoDetectSourceLanguageconfig, const char* languages)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, languages == nullptr || !(*languages));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hAutoDetectSourceLanguageconfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hAutoDetectSourceLanguageconfig = SPXHANDLE_INVALID;

        auto autoDetectSourceLangConfig = SpxCreateObjectWithSite<ISpxAutoDetectSourceLangConfig>("CSpxAutoDetectSourceLangConfig", SpxGetRootSite());
        autoDetectSourceLangConfig->InitFromLanguages(languages);

        auto autoDetectSourceLangConfigs = CSpxSharedPtrHandleTableManager::Get<ISpxAutoDetectSourceLangConfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE>();
        *hAutoDetectSourceLanguageconfig = autoDetectSourceLangConfigs->TrackHandle(autoDetectSourceLangConfig);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI create_auto_detect_source_lang_config_from_source_lang_config(SPXAUTODETECTSOURCELANGCONFIGHANDLE* hAutoDetectSourceLanguageconfig, SPXSOURCELANGCONFIGHANDLE hSourceLanguageConfig)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hSourceLanguageConfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hAutoDetectSourceLanguageconfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hAutoDetectSourceLanguageconfig = SPXHANDLE_INVALID;

        auto autoDetectSourceLangConfig = SpxCreateObjectWithSite<ISpxAutoDetectSourceLangConfig>("CSpxAutoDetectSourceLangConfig", SpxGetRootSite());
        auto sourceLangConfigs = CSpxSharedPtrHandleTableManager::Get<ISpxSourceLanguageConfig, SPXSOURCELANGCONFIGHANDLE>();
        auto sourceLangConfigPtr = (*sourceLangConfigs)[hSourceLanguageConfig];
        auto sourceLanguageConfig = SpxQueryInterface<ISpxSourceLanguageConfig>(sourceLangConfigPtr);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, sourceLanguageConfig == nullptr);

        autoDetectSourceLangConfig->AddSourceLanguageConfig(sourceLanguageConfig);

         auto autoDetectSourceLangConfigs = CSpxSharedPtrHandleTableManager::Get<ISpxAutoDetectSourceLangConfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE>();
        *hAutoDetectSourceLanguageconfig = autoDetectSourceLangConfigs->TrackHandle(autoDetectSourceLangConfig);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI add_source_lang_config_to_auto_detect_source_lang_config(SPXAUTODETECTSOURCELANGCONFIGHANDLE hAutoDetectSourceLanguageconfig, SPXSOURCELANGCONFIGHANDLE hSourceLanguageConfig)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hSourceLanguageConfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hAutoDetectSourceLanguageconfig == SPXHANDLE_INVALID);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto sourceLangConfigs = CSpxSharedPtrHandleTableManager::Get<ISpxSourceLanguageConfig, SPXSOURCELANGCONFIGHANDLE>();
        auto sourceLangConfigPtr = (*sourceLangConfigs)[hSourceLanguageConfig];
        auto sourceLanguageConfig = SpxQueryInterface<ISpxSourceLanguageConfig>(sourceLangConfigPtr);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, sourceLanguageConfig == nullptr);

        auto autoDetectSourceLangConfigs = CSpxSharedPtrHandleTableManager::Get<ISpxAutoDetectSourceLangConfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE>();
        auto autoDetectSourceLangConfigPtr = (*autoDetectSourceLangConfigs)[hAutoDetectSourceLanguageconfig];
        auto autoDetectSourceLangConfig = SpxQueryInterface<ISpxAutoDetectSourceLangConfig>(autoDetectSourceLangConfigPtr);

        autoDetectSourceLangConfig->AddSourceLanguageConfig(sourceLanguageConfig);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_(bool) auto_detect_source_lang_config_is_handle_valid(SPXAUTODETECTSOURCELANGCONFIGHANDLE hAutoDetectSourceLanguageconfig)
{
    return CSpxApiManager::IsValid<SPXAUTODETECTSOURCELANGCONFIGHANDLE, ISpxAutoDetectSourceLangConfig>(hAutoDetectSourceLanguageconfig);
}

SPXAPI auto_detect_source_lang_config_release(SPXAUTODETECTSOURCELANGCONFIGHANDLE hAutoDetectSourceLanguageconfig)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXAUTODETECTSOURCELANGCONFIGHANDLE, ISpxAutoDetectSourceLangConfig>(hAutoDetectSourceLanguageconfig);
}

SPXAPI auto_detect_source_lang_config_get_property_bag(SPXAUTODETECTSOURCELANGCONFIGHANDLE hAutoDetectSourceLanguageconfig, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXAUTODETECTSOURCELANGCONFIGHANDLE, ISpxAutoDetectSourceLangConfig, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hAutoDetectSourceLanguageconfig, hpropbag);
}
