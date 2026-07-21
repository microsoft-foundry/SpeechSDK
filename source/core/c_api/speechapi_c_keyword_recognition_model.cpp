//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_keyword_recognition_model.cpp: Public API definitions for KeywordRecognitionModel related C methods
//

#include "stdafx.h"
#include "create_object_helpers.h"
#include "handle_helpers.h"
#include "resource_manager.h"


using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;


SPXAPI_(bool) keyword_recognition_model_handle_is_valid(SPXKEYWORDHANDLE hkeyword)
{
    return CSpxApiManager::IsValid<SPXKEYWORDHANDLE, ISpxKwsModel>(hkeyword);
}

SPXAPI keyword_recognition_model_handle_release(SPXKEYWORDHANDLE hkeyword)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXKEYWORDHANDLE, ISpxKwsModel>(hkeyword);
}

SPXAPI keyword_recognition_model_create_from_file(const char* fileName, SPXKEYWORDHANDLE* phkwmodel)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phkwmodel == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, fileName == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phkwmodel = SPXHANDLE_INVALID;

        auto model = SpxCreateObjectWithSite<ISpxKwsModel>("CSpxKwsModel", SpxGetRootSite());
        model->InitFromFile(PAL::ToWString(fileName).c_str());

        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxKwsModel, SPXKEYWORDHANDLE>();
        *phkwmodel = handles->TrackHandle(model);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI keyword_recognition_model_create_from_config(SPXSPEECHCONFIGHANDLE hspeechconfig, SPXKEYWORDHANDLE* phkeyword)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hspeechconfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hspeechconfig == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phkeyword == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phkeyword = SPXHANDLE_INVALID;

        auto configs = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*configs)[hspeechconfig];

        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        auto configPropertyBag = SpxQueryInterface<ISpxNamedProperties>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, configPropertyBag == nullptr);

        auto model = SpxCreateObjectWithSite<ISpxKwsModel>("CSpxKwsModel", SpxGetRootSite());

        // Verify that the specified model exists.
        auto modelName = configPropertyBag->GetStringValue(GetPropertyName(PropertyId::KeywordRecognition_ModelName));
        auto keywordRecoModel = embeddedConfig->GetKeywordRecognitionModel(modelName);

        if (keywordRecoModel != nullptr)
        {
            auto modelPropertyBag = SpxQueryInterface<ISpxNamedProperties>(model);
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, modelPropertyBag == nullptr);

            // Copy model related settings from config.
            auto modelKey = configPropertyBag->GetStringValue(GetPropertyName(PropertyId::KeywordRecognition_ModelKey));
            modelPropertyBag->Set(PropertyId::KeywordRecognition_ModelKey, modelKey.c_str());
            modelPropertyBag->SetStringValue(g_keywordRecognitionModelPath, keywordRecoModel->GetPath().c_str());

            modelPropertyBag->SetStringValue(g_isMultiKeywordRecognition, "true");
        }
        else
        {
            std::string errorMsg =
                "Cannot find an embedded keyword recognition model by name \"" + modelName + "\". "
                "Check that the arguments for EmbeddedSpeechConfig::FromPath or FromPaths are valid "
                "model paths and SetKeywordRecognitionModel is called with a valid model name.";
            ThrowInvalidArgumentException(errorMsg);
        }

        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxKwsModel, SPXKEYWORDHANDLE>();
        *phkeyword = handles->TrackHandle(model);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI keyword_recognition_model_add_user_defined_wake_word(SPXKEYWORDHANDLE hkeyword, const char* wakeword)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hkeyword == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hkeyword == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, wakeword == nullptr || !(*wakeword));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxKwsModel, SPXKEYWORDHANDLE>();
        auto handle = (*handles)[hkeyword];

        auto model = SpxQueryInterface<ISpxKwsModel>(handle);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, model == nullptr);

        auto model_property_bag = SpxQueryInterface<ISpxNamedProperties>(model);
        auto propertyString = model_property_bag->GetStringValue(g_keywordRecognitionUserDefinedWakeWords, "");

        if (propertyString.empty())
        {
            propertyString = wakeword;
        }
        else
        {
            propertyString.append(",");
            propertyString.append(wakeword);
        }

        model_property_bag->SetStringValue(g_keywordRecognitionUserDefinedWakeWords, propertyString.c_str());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
