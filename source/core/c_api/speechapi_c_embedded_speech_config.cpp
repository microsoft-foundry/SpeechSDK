//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_embedded_speech_config.cpp: Definitions for EmbeddedSpeechConfig related C methods
//

#include "stdafx.h"
#include "speechapi_c_embedded_speech_config.h"
#include "create_object_helpers.h"
#include "handle_helpers.h"
#include "site_helpers.h"
#include "property_id_2_name_map.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI embedded_speech_config_create(SPXSPEECHCONFIGHANDLE* hconfig)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hconfig = SPXHANDLE_INVALID;

        // Note, the use of ISpxSpeechConfig here is intentional.
        auto config = SpxCreateObjectWithSite<ISpxSpeechConfig>("CSpxEmbeddedSpeechConfig", SpxGetRootSite());
        config->InitEmbedded();

        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        *hconfig = confighandles->TrackHandle(config);

        auto econfig = (*confighandles)[*hconfig];

        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(econfig);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        embeddedConfig->Init();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI embedded_speech_config_add_path(SPXSPEECHCONFIGHANDLE hconfig, const char* path)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, path == nullptr || !(*path));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto configs = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*configs)[hconfig];

        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        embeddedConfig->AddSearchPath(path);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI embedded_speech_config_get_num_speech_reco_models(SPXSPEECHCONFIGHANDLE hconfig, uint32_t* numModels)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, numModels == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto configs = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*configs)[hconfig];

        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        *numModels = embeddedConfig->GetNumSpeechRecognitionModels();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI embedded_speech_config_get_speech_reco_model(SPXSPEECHCONFIGHANDLE hconfig, uint32_t index, SPXSPEECHRECOMODELHANDLE* hmodel)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hmodel == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto configs = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*configs)[hconfig];

        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        auto model = embeddedConfig->GetSpeechRecognitionModel(index);
        SPX_THROW_HR_IF(SPXERR_NOT_FOUND, model == nullptr);

        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechRecognitionModel, SPXSPEECHRECOMODELHANDLE>();
        *hmodel = handles->TrackHandle(model);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI embedded_speech_config_get_num_speech_translation_models(SPXSPEECHCONFIGHANDLE hconfig, uint32_t* numModels)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, numModels == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto configs = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*configs)[hconfig];

        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        *numModels = embeddedConfig->GetNumSpeechTranslationModels();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI embedded_speech_config_get_speech_translation_model(SPXSPEECHCONFIGHANDLE hconfig, uint32_t index, SPXSPEECHRECOMODELHANDLE* hmodel)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hmodel == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto configs = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*configs)[hconfig];

        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        auto model = embeddedConfig->GetSpeechTranslationModel(index);
        SPX_THROW_HR_IF(SPXERR_NOT_FOUND, model == nullptr);

        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechTranslationModel, SPXSPEECHRECOMODELHANDLE>();
        *hmodel = handles->TrackHandle(model);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI embedded_speech_config_set_model(SPXSPEECHCONFIGHANDLE hconfig, PropertyId namePropId, const char* name, PropertyId licensePropId, const char* license)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, license == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto configs = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*configs)[hconfig];

        auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(config);
        namedProperties->Set(namePropId, name);
        namedProperties->Set(licensePropId, license);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI embedded_speech_config_set_speech_recognition_model(SPXSPEECHCONFIGHANDLE hconfig, const char* name, const char* license)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, name == nullptr || !(*name));

    return embedded_speech_config_set_model(hconfig,
        PropertyId::SpeechServiceConnection_RecoModelName, name,
        PropertyId::SpeechServiceConnection_RecoModelKey, license);
}

SPXAPI embedded_speech_config_set_speech_synthesis_voice(SPXSPEECHCONFIGHANDLE hconfig, const char* name, const char* license)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, name == nullptr); // can be empty string

    return embedded_speech_config_set_model(hconfig,
        PropertyId::SpeechServiceConnection_SynthOfflineVoice, name,
        PropertyId::SpeechServiceConnection_SynthModelKey, license);
}

SPXAPI embedded_speech_config_set_speech_translation_model(SPXSPEECHCONFIGHANDLE hconfig, const char* name, const char* license)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, name == nullptr || !(*name));

    return embedded_speech_config_set_model(hconfig,
        PropertyId::SpeechTranslation_ModelName, name,
        PropertyId::SpeechTranslation_ModelKey, license);
}

SPXAPI embedded_speech_config_set_keyword_recognition_model(SPXSPEECHCONFIGHANDLE hconfig, const char* name, const char* license)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, name == nullptr || !(*name));

    return embedded_speech_config_set_model(hconfig,
        PropertyId::KeywordRecognition_ModelName, name,
        PropertyId::KeywordRecognition_ModelKey, license);
}
