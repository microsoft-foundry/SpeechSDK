//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_factory.cpp: Definitions for SpeechFactory related C methods
//

#include <string>
#include <vector>

#include "stdafx.h"
#include "service_helpers.h"
#include "site_helpers.h"
#include "handle_helpers.h"
#include "resource_manager.h"
#include "mock_controller.h"
#include "property_id_2_name_map.h"
#include "speechapi_c_speech_config.h"
#include "speechapi_c_auto_detect_source_lang_config.h"
#include "speechapi_c_source_lang_config.h"
#include "memory_utils.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace std;

static_assert((int)OutputFormat::Simple == (int)SpeechOutputFormat_Simple, "OutputFormat should match between C and C++ layers");
static_assert((int)OutputFormat::Detailed == (int)SpeechOutputFormat_Detailed, "OutputFormat should match between C and C++ layers");

/*
 * In the end there will only be once instance of this template as all the functions have
 * the same signature and all the handle types are aliases of SPXHANDLE
 */
template<typename T, typename Function, typename Handle>
std::shared_ptr<T> ObjectOrEmptyIfInvalid(Function fn, Handle handle)
{
    return fn(handle) ? CSpxSharedPtrHandleTableManager::GetPtr<T, Handle>(handle) : nullptr;
}

std::shared_ptr<ISpxSpeechConfig> SpeechConfigFromHandleOrEmptyIfInvalid(SPXSPEECHCONFIGHANDLE hconfig)
{
    return ObjectOrEmptyIfInvalid<ISpxSpeechConfig>(speech_config_is_handle_valid, hconfig);
}

std::shared_ptr<ISpxAudioConfig> AudioConfigFromHandleOrEmptyIfInvalid(SPXAUDIOCONFIGHANDLE haudioConfig)
{
    return ObjectOrEmptyIfInvalid<ISpxAudioConfig>(audio_config_is_handle_valid, haudioConfig);
}

std::shared_ptr<ISpxAutoDetectSourceLangConfig> AutoDetectSourceLangConfigFromHandleOrEmptyIfInvalid(SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig)
{
    return ObjectOrEmptyIfInvalid<ISpxAutoDetectSourceLangConfig>(
        auto_detect_source_lang_config_is_handle_valid,
        hautoDetectSourceLangConfig);
}

std::shared_ptr<ISpxSourceLanguageConfig> SourceLangConfigFromHandleOrEmptyIfInvalid(SPXSOURCELANGCONFIGHANDLE hSourceLangConfig)
{
    return ObjectOrEmptyIfInvalid<ISpxSourceLanguageConfig>(
        source_lang_config_is_handle_valid,
        hSourceLangConfig);
}

template<typename FactoryMethod>
auto create_from_config(SPXHANDLE hspeechconfig, SPXHANDLE hautoDetectSourceLangConfig, SPXHANDLE hSourceLangConfig, SPXHANDLE haudioConfig, FactoryMethod fm)
{
    auto factory = SpxCreateObjectWithSite<ISpxSpeechApiFactory>("CSpxSpeechApiFactory", SpxGetRootSite());
    SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, factory == nullptr);

    auto factory_property_bag = SpxQueryInterface<ISpxNamedProperties>(factory);

    auto config = SpeechConfigFromHandleOrEmptyIfInvalid(hspeechconfig);
    auto config_property_bag = SpxQueryInterface<ISpxNamedProperties>(config);

    if (config != nullptr)
    {
        Memory::CheckObjectCount(hspeechconfig);

        //copy the properties from the speech config into the factory
        if (config_property_bag != nullptr)
        {
            factory_property_bag->Copy(config_property_bag, false);
        }
    }

    auto audio_input = AudioConfigFromHandleOrEmptyIfInvalid(haudioConfig);
    // copy the audio input properties into the factory, if any.
    auto audio_input_properties = SpxQueryInterface<ISpxNamedProperties>(audio_input);
    if (audio_input_properties != nullptr)
    {
        factory_property_bag->Copy(audio_input_properties, false);
    }

    auto auto_detect_source_lang_config = AutoDetectSourceLangConfigFromHandleOrEmptyIfInvalid(hautoDetectSourceLangConfig);
    // copy the auto detect source language config properties into the factory, if any.
    auto auto_detect_source_lang_config_properties = SpxQueryInterface<ISpxNamedProperties>(auto_detect_source_lang_config);
    if (auto_detect_source_lang_config_properties != nullptr)
    {
        if (config_property_bag != nullptr && config_property_bag->Get(PropertyId::SpeechServiceConnection_EndpointId))
        {
            ThrowInvalidArgumentException("EndpointId on SpeechConfig is unsupported for auto detection source language scenario. "
                "Please set per language endpointId through SourceLanguageConfig and use it to construct AutoDetectSourceLanguageConfig.");
        }

        if (auto_detect_source_lang_config_properties->GetOr(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, "")
            == g_autoDetectSourceLang_OpenRange)
        {
            bool isMRS = config_property_bag != nullptr &&
                config_property_bag->GetOr(PropertyId::SpeechServiceResponse_PostProcessingOption, "") == "PostRefinement";

            if (!isMRS)
            {
                ThrowInvalidArgumentException("Recognizer doesn't support auto detection source language from open range. "
                    "Please set specific languages using AutoDetectSourceLanguageConfig::FromLanguages() or AutoDetectSourceLanguageConfig::FromSourceLanguageConfigs()");
            }
        }
        factory_property_bag->Copy(auto_detect_source_lang_config_properties, false);
    }

    auto source_lang_config = SourceLangConfigFromHandleOrEmptyIfInvalid(hSourceLangConfig);
    // copy the source language config properties into the factory, if any.
    auto source_lang_config_properties = SpxQueryInterface<ISpxNamedProperties>(source_lang_config);
    if (source_lang_config_properties != nullptr)
    {
        factory_property_bag->Copy(source_lang_config_properties, false);
    }

    // Check if we have EmbeddedSpeechConfig instead of SpeechConfig
    auto useEmbeddedConfig =
        config_property_bag != nullptr ?
        PAL::ToBool(config_property_bag->GetStringValue("CARBON-INTERNAL-UseEmbeddedSpeechConfig")) :
        false;
    if (useEmbeddedConfig)
    {
        // Get the speech recognition model info
        auto speechRecoModelName = config_property_bag->GetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_RecoModelName));
        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        auto speechRecoModel = embeddedConfig->GetSpeechRecognitionModel(speechRecoModelName);
        if (speechRecoModel != nullptr)
        {
            factory_property_bag->SetStringValue("SpeechRecognition_ModelPath", speechRecoModel->GetPath().c_str());
            factory_property_bag->SetStringValue("SpeechRecognition_ModelLocales",
                PAL::Join(speechRecoModel->GetLocales(), std::string(1, ',').c_str()).c_str());
        }
        else
        {
            std::string errorMsg =
                "Cannot find an embedded speech recognition model by name \"" +
                speechRecoModelName + "\". Check that the arguments for FromPath "
                "or FromPaths are valid model paths and SetSpeechRecognitionModel "
                "is called with a valid model name.";
            ThrowInvalidArgumentException(errorMsg);
        }
    }

    return ((*factory).*fm)(audio_input);
}

SPXAPI recognizer_create_speech_recognizer_from_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, SPXHANDLE_INVALID, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateSpeechRecognizerFromConfig);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(recognizer);
        std::string unsupportedV2ServiceProperties = properties->GetStringValue(g_unsupportedV2ServiceProperties, "");
        bool isCustomV1Endpoint = properties->GetOr<bool>(g_isCustomV1Endpoint, false);
        if (!isCustomV1Endpoint && !unsupportedV2ServiceProperties.empty())
        {
            std::string errorMessage = "Setting these parameters as service properties is no longer supported for SpeechRecognizer: " + unsupportedV2ServiceProperties + ". Please use corresponding API functions.";
            ThrowLogicError(errorMessage);
        }
        // track the reco handle
        auto recohandles  = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_speech_recognizer_from_auto_detect_source_lang_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !auto_detect_source_lang_config_is_handle_valid(hautoDetectSourceLangConfig));
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, hautoDetectSourceLangConfig, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateSpeechRecognizerFromConfig);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(recognizer);
        std::string unsupportedV2ServiceProperties = properties->GetStringValue(g_unsupportedV2ServiceProperties, "");
        bool isCustomV1Endpoint = properties->GetOr<bool>(g_isCustomV1Endpoint, false);
        if (!isCustomV1Endpoint && !unsupportedV2ServiceProperties.empty())
        {
            std::string errorMessage = "Setting these parameters as service properties is no longer supported for SpeechRecognizer: " + unsupportedV2ServiceProperties + ". Please use corresponding API functions.";
            ThrowLogicError(errorMessage);
        }
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_speech_recognizer_from_source_lang_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXSOURCELANGCONFIGHANDLE hSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !source_lang_config_is_handle_valid(hSourceLangConfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, SPXHANDLE_INVALID, hSourceLangConfig, haudioInput, &ISpxSpeechApiFactory::CreateSpeechRecognizerFromConfig);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(recognizer);
        std::string unsupportedV2ServiceProperties = properties->GetStringValue(g_unsupportedV2ServiceProperties, "");
        bool isCustomV1Endpoint = properties->GetOr<bool>(g_isCustomV1Endpoint, false);
        if (!isCustomV1Endpoint && !unsupportedV2ServiceProperties.empty())
        {
            std::string errorMessage = "Setting these parameters as service properties is no longer supported for SpeechRecognizer: " + unsupportedV2ServiceProperties + ". Please use corresponding API functions.";
            ThrowLogicError(errorMessage);
        }
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_source_language_recognizer_from_auto_detect_source_lang_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !auto_detect_source_lang_config_is_handle_valid(hautoDetectSourceLangConfig));
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        Memory::CheckObjectCount(hspeechconfig);

        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, hautoDetectSourceLangConfig, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateSourceLanguageRecognizerFromConfig);
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI dialog_service_connector_create_dialog_service_connector_from_config(SPXRECOHANDLE* ph_dialog_service_connector, SPXSPEECHCONFIGHANDLE h_dialog_service_config, SPXAUDIOCONFIGHANDLE h_audio_input)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, ph_dialog_service_connector == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(h_dialog_service_config));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *ph_dialog_service_connector = SPXHANDLE_INVALID;

        Memory::CheckObjectCount(h_dialog_service_config);

        // Enable keyword verification for dialog service connector by default
        auto config_handles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*config_handles)[h_dialog_service_config];
        auto config_property_bag = SpxQueryInterface<ISpxNamedProperties>(config);

        auto enableKeywordVerification = config_property_bag->GetStringValue(KeywordConfig_EnableKeywordVerification, "true");
        config_property_bag->SetStringValue(KeywordConfig_EnableKeywordVerification, enableKeywordVerification.c_str());

        auto connector = create_from_config(h_dialog_service_config, SPXHANDLE_INVALID, SPXHANDLE_INVALID, h_audio_input, &ISpxSpeechApiFactory::CreateDialogServiceConnectorFromConfig);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(connector);
        properties->SetStringValue(g_isDialogServiceConnector, "true");

        // track the handle
        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxDialogServiceConnector, SPXRECOHANDLE>();
        *ph_dialog_service_connector = handles->TrackHandle(connector);

    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

bool check_for_embedded_speech_translation(
    std::shared_ptr<ISpxSpeechConfig> speechConfig,
    std::shared_ptr<ISpxNamedProperties> factoryProperties)
{
    // Check if we have EmbeddedSpeechConfig instead of SpeechConfig
    auto speechConfigProperties = SpxQueryInterface<ISpxNamedProperties>(speechConfig);
    auto useEmbeddedConfig =
        speechConfigProperties != nullptr ?
        PAL::ToBool(speechConfigProperties->GetStringValue("CARBON-INTERNAL-UseEmbeddedSpeechConfig")) :
        false;

    if (useEmbeddedConfig)
    {
        // Get the speech translation model info
        auto modelName = speechConfigProperties->GetStringValue(GetPropertyName(PropertyId::SpeechTranslation_ModelName));
        auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(speechConfig);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);

        auto model = embeddedConfig->GetSpeechTranslationModel(modelName);
        if (model != nullptr)
        {
            factoryProperties->SetStringValue("SpeechTranslation_ModelPath", model->GetPath().c_str());
            factoryProperties->SetStringValue("SpeechTranslation_ModelTargetLanguages",
                PAL::Join(model->GetTargetLanguages(), std::string(1, ',').c_str()).c_str());
            factoryProperties->SetStringValue("SpeechTranslation_ModelDefaultTargetLanguage",
                model->GetDefaultTargetLanguage().c_str());
        }
        else
        {
            std::string errorMsg =
                "Cannot find an embedded speech translation model by name \"" + modelName + "\"."
                " Check that the arguments for FromPath or FromPaths are valid model paths"
                " and SetSpeechTranslationModel is called with a valid model name.";
            ThrowInvalidArgumentException(errorMsg);
        }
    }

    return useEmbeddedConfig;
}

SPXAPI recognizer_create_translation_recognizer_from_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;

        Memory::CheckObjectCount(hspeechconfig);

        std::shared_ptr<ISpxRecognizer> recognizer;

        // create a factory
        auto factory = SpxCreateObjectWithSite<ISpxSpeechApiFactory>("CSpxSpeechApiFactory", SpxGetRootSite());

        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto speechconfig = (*confighandles)[hspeechconfig];

        //copy the properties from the speech config into the factory
        auto speechconfig_propertybag = SpxQueryInterface<ISpxNamedProperties>(speechconfig);
        auto fbag = SpxQueryInterface<ISpxNamedProperties>(factory);
        fbag->Copy(speechconfig_propertybag, false);

        auto audioInput = AudioConfigFromHandleOrEmptyIfInvalid(haudioInput);
        // copy the audio input properties into the factory, if any.
        auto audioInput_propertybag = SpxQueryInterface<ISpxNamedProperties>(audioInput);
        if (audioInput_propertybag != nullptr)
        {
            fbag->Copy(audioInput_propertybag, false);
        }

        check_for_embedded_speech_translation(speechconfig, fbag);

        recognizer = factory->CreateTranslationRecognizerFromConfig(audioInput);

        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_translation_recognizer_from_auto_detect_source_lang_config(
        SPXRECOHANDLE* phreco,
        SPXSPEECHCONFIGHANDLE hspeechconfig,
        SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig,
        SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !auto_detect_source_lang_config_is_handle_valid(hautoDetectSourceLangConfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;

        Memory::CheckObjectCount(hspeechconfig);

        std::shared_ptr<ISpxRecognizer> recognizer;

        // Create a factory
        auto factory = SpxCreateObjectWithSite<ISpxSpeechApiFactory>("CSpxSpeechApiFactory", SpxGetRootSite());

        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto speechconfig = (*confighandles)[hspeechconfig];

        // Copy the properties from the speech config into the factory
        auto speechconfig_propertybag = SpxQueryInterface<ISpxNamedProperties>(speechconfig);
        auto fbag = SpxQueryInterface<ISpxNamedProperties>(factory);
        fbag->Copy(speechconfig_propertybag, false);

        auto isEmbeddedTranslation = check_for_embedded_speech_translation(speechconfig, fbag);

        auto auto_detect_source_lang_config = AutoDetectSourceLangConfigFromHandleOrEmptyIfInvalid(hautoDetectSourceLangConfig);
        // Copy the auto detect source language config properties into the factory, if any.
        auto auto_detect_source_lang_config_properties = SpxQueryInterface<ISpxNamedProperties>(auto_detect_source_lang_config);
        if (auto_detect_source_lang_config_properties != nullptr)
        {
            if (speechconfig_propertybag != nullptr && speechconfig_propertybag->Get(PropertyId::SpeechServiceConnection_EndpointId))
            {
                ThrowInvalidArgumentException("EndpointId on SpeechConfig is unsupported for auto detection source language scenario. "
                    "Please set per language endpointId through SourceLanguageConfig and use it to construct AutoDetectSourceLanguageConfig.");
            }

            std::string autoDetectSourceLanguages = auto_detect_source_lang_config_properties->GetOr(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, "");
            if (autoDetectSourceLanguages.compare(g_autoDetectSourceLang_OpenRange) != 0 && isEmbeddedTranslation && !autoDetectSourceLanguages.empty())
            {
                ThrowInvalidArgumentException(
                    "Embedded speech translation supports source language auto detection with AutoDetectSourceLanguageConfig::FromOpenRange() only.");
            }
            fbag->Copy(auto_detect_source_lang_config_properties, true);
        }

        auto audioInput = AudioConfigFromHandleOrEmptyIfInvalid(haudioInput);
        // Copy the audio input properties into the factory, if any.
        auto audioInput_propertybag = SpxQueryInterface<ISpxNamedProperties>(audioInput);
        if (audioInput_propertybag != nullptr)
        {
            fbag->Copy(audioInput_propertybag, false);
        }
        recognizer = factory->CreateTranslationRecognizerFromConfig(audioInput);

        // Track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_keyword_recognizer_from_audio_config(SPXRECOHANDLE* phreco, SPXAUDIOCONFIGHANDLE haudio)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(SPXHANDLE_INVALID, SPXHANDLE_INVALID, SPXHANDLE_INVALID, haudio, &ISpxSpeechApiFactory::CreateSpeechRecognizerFromConfig);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(recognizer);
        properties->SetStringValue(g_keyword_KeywordOnly, "true");
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

template<typename FactoryMethod>
auto create_synthesizer_from_config(SPXHANDLE hspeechconfig, SPXHANDLE hautoDetectSourceLangConfig, SPXHANDLE haudioConfig, FactoryMethod fm)
{
    const auto factory = SpxCreateObjectWithSite<ISpxSpeechSynthesisApiFactory>("CSpxSpeechSynthesisApiFactory", SpxGetRootSite());
    SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, factory == nullptr);

    auto factory_property_bag = SpxQueryInterface<ISpxNamedProperties>(factory);

    const auto config = SpeechConfigFromHandleOrEmptyIfInvalid(hspeechconfig);
    const auto config_property_bag = SpxQueryInterface<ISpxNamedProperties>(config);

    if (config != nullptr)
    {
        Memory::CheckObjectCount(hspeechconfig);

        // Copy the properties from the speech config into the factory
        if (config_property_bag != nullptr)
        {
            factory_property_bag->Copy(config_property_bag, false);
        }
    }

    auto audio_output = AudioConfigFromHandleOrEmptyIfInvalid(haudioConfig);
    // copy the audio output properties into the factory, if any.
    const auto audio_output_properties = SpxQueryInterface<ISpxNamedProperties>(audio_output);
    if (audio_output_properties != nullptr)
    {
        factory_property_bag->Copy(audio_output_properties, false);
    }

    const auto auto_detect_source_lang_config = AutoDetectSourceLangConfigFromHandleOrEmptyIfInvalid(hautoDetectSourceLangConfig);
    // copy the auto detect source language config properties into the factory, if any.
    const auto auto_detect_source_lang_config_properties = SpxQueryInterface<ISpxNamedProperties>(auto_detect_source_lang_config);
    if (auto_detect_source_lang_config_properties != nullptr)
    {
        if (auto_detect_source_lang_config_properties->GetOr(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, "") != g_autoDetectSourceLang_OpenRange)
        {
            ThrowInvalidArgumentException("Auto detection source languages in SpeechSynthesizer doesn't support language range specification. "
                "Please use FromOpenRange to construct AutoDetectSourceLanguageConfig.");
        }
        factory_property_bag->Copy(auto_detect_source_lang_config_properties, false);
    }

    auto useEmbeddedConfig =
        config_property_bag != nullptr ?
        PAL::ToBool(config_property_bag->GetStringValue("CARBON-INTERNAL-UseEmbeddedSpeechConfig")) :
        false;
    if (useEmbeddedConfig)
    {
        auto modelPathList = config_property_bag->GetOr(PropertyId::SpeechServiceConnection_SynthOfflineDataPath, "");
        if (modelPathList.empty())
        {
            auto embeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, embeddedConfig == nullptr);
            modelPathList = embeddedConfig->GetSearchPathList();
        }
        factory_property_bag->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_SynthOfflineDataPath), modelPathList.c_str());
    }

    return ((*factory).*fm)(audio_output);
}

SPXAPI synthesizer_create_speech_synthesizer_from_config(SPXSYNTHHANDLE* phsynth, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioOutput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phsynth == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phsynth = SPXHANDLE_INVALID;
        const auto synthesizer = create_synthesizer_from_config(hspeechconfig, SPXHANDLE_INVALID, haudioOutput, &ISpxSpeechSynthesisApiFactory::CreateSpeechSynthesizerFromConfig);
        // track the synth handle
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        *phsynth = synthhandles->TrackHandle(synthesizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_create_speech_synthesizer_from_auto_detect_source_lang_config(SPXSYNTHHANDLE* phsynth, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioOutput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phsynth == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !auto_detect_source_lang_config_is_handle_valid(hautoDetectSourceLangConfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phsynth = SPXHANDLE_INVALID;
        const auto synthesizer = create_synthesizer_from_config(hspeechconfig, hautoDetectSourceLangConfig, haudioOutput, &ISpxSpeechSynthesisApiFactory::CreateSpeechSynthesizerFromConfig);
        // track the synth handle
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        *phsynth = synthhandles->TrackHandle(synthesizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

std::shared_ptr<ISpxSpeechApiFactory> create_factory_from_speech_config(SPXSPEECHCONFIGHANDLE hspeechconfig)
{
    if (!speech_config_is_handle_valid(hspeechconfig))
    {
        throw std::runtime_error("Invalid speechconfig handle.");
    }

    Memory::CheckObjectCount(hspeechconfig);

    // get the input parameters from the hspeechconfig
    auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
    auto speechconfig = (*confighandles)[hspeechconfig];
    auto speechconfig_propertybag = SpxQueryInterface<ISpxNamedProperties>(speechconfig);
    auto factory = SpxCreateObjectWithSite<ISpxSpeechApiFactory>("CSpxSpeechApiFactory", SpxGetRootSite());
    SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, factory == nullptr);

    //copy the properties from the speech config into the factory
    auto fbag = SpxQueryInterface<ISpxNamedProperties>(factory);
    if (speechconfig_propertybag != nullptr)
    {
        fbag->Copy(speechconfig_propertybag, false);
    }

    return factory;
}

SPXAPI meeting_create_from_config(SPXMEETINGHANDLE* pmeeting, SPXSPEECHCONFIGHANDLE hspeechconfig, const char* id)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pmeeting == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    // id is optional
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, id == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *pmeeting = SPXHANDLE_INVALID;

        auto factory = create_factory_from_speech_config(hspeechconfig);

        auto meeting = factory->CreateMeetingFromConfig(id);

        // track the meeting handle
        auto meetinghandles = CSpxSharedPtrHandleTableManager::Get<ISpxMeeting, SPXMEETINGHANDLE>();
        *pmeeting = meetinghandles->TrackHandle(meeting);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_conversation_transcriber_from_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, SPXHANDLE_INVALID, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateConversationTranscriberV2FromConfig);

        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(recognizer);
        properties->SetStringValue(g_isConversationTranscriber_V2, "true");
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_conversation_transcriber_from_auto_detect_source_lang_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !auto_detect_source_lang_config_is_handle_valid(hautoDetectSourceLangConfig));
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, hautoDetectSourceLangConfig, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateConversationTranscriberV2FromConfig);
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(recognizer);
        properties->SetStringValue(g_isConversationTranscriber_V2, "true");
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_conversation_transcriber_from_source_lang_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXSOURCELANGCONFIGHANDLE hSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !source_lang_config_is_handle_valid(hSourceLangConfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, SPXHANDLE_INVALID, hSourceLangConfig, haudioInput, &ISpxSpeechApiFactory::CreateConversationTranscriberV2FromConfig);
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(recognizer);
        properties->SetStringValue(g_isConversationTranscriber_V2, "true");
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_meeting_transcriber_from_config(SPXRECOHANDLE* phreco, SPXAUDIOCONFIGHANDLE haudioinput)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;

        auto meeting_transcriber = SpxCreateObject<ISpxRecognizer>("CSpxMeetingTranscriber", SpxGetRootSite());

        auto meeting_transcriber_init = SpxQueryInterface<ISpxObjectWithAudioConfig>(meeting_transcriber);
        SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, meeting_transcriber_init == nullptr);

        auto audioInput = AudioConfigFromHandleOrEmptyIfInvalid(haudioinput);
        meeting_transcriber_init->SetAudioConfig(audioInput);

        // Meeting Transcriber has a unique object & site hierarchy that makes it important to copy
        // property data from any input config.
        auto audioProperties = SpxQueryInterface<ISpxNamedProperties>(audioInput);
        auto transcriberProperties = SpxQueryInterface<ISpxNamedProperties>(meeting_transcriber);
        if (audioProperties && transcriberProperties)
        {
            transcriberProperties->Copy(audioProperties, true);
        }

        // track the meeting transcriber handle
        auto transcribers = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = transcribers->TrackHandle(meeting_transcriber);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_join_meeting(SPXMEETINGHANDLE hmeeting, SPXRECOHANDLE hreco)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hmeeting == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto meetinghandles = CSpxSharedPtrHandleTableManager::Get<ISpxMeeting, SPXMEETINGHANDLE>();
        auto meeting = (*meetinghandles)[hmeeting];

        auto transcribers = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        auto meeting_transcriber = (*transcribers)[hreco];

        auto factory = SpxQueryService<ISpxSpeechApiFactory>(meeting);
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, factory == nullptr);

        auto factoryProps = SpxQueryService<ISpxNamedProperties>(factory);
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, factoryProps == nullptr);

        auto session = SpxQueryService<ISpxSession>(meeting);
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, session == nullptr);

        auto session_as_site = SpxQueryInterface<ISpxGenericSite>(session);

        auto meeting_transcriber_set_site = SpxQueryInterface<ISpxObjectWithSite>(meeting_transcriber);
        meeting_transcriber_set_site->SetSite(session_as_site);

        // hook audio input to session
        auto objectWithAudioConfig = SpxQueryInterface<ISpxObjectWithAudioConfig>(meeting_transcriber);
        auto audioConfig = objectWithAudioConfig->GetAudioConfig();
        auto audioConfigProperties = SpxQueryInterface<ISpxNamedProperties>(audioConfig);
        if (audioConfigProperties != nullptr)
        {
            factoryProps->Copy(audioConfigProperties, false);
        }

        factory->InitSessionFromAudioInputConfig(SpxQueryInterface<ISpxAudioStreamSessionInit>(session), audioConfig);

        auto properties = SpxQueryInterface<ISpxNamedProperties>(meeting_transcriber);
        properties->SetStringValue(g_isMeetingTranscriber, "true");

        // hook meeting to meeting transcriber, so that I can get the participant list later
        auto transcriber_ptr = SpxQueryInterface<ISpxMeetingTranscriber>(meeting_transcriber);
        transcriber_ptr->JoinMeeting(meeting);

        // hook the transcriber to session
        session->AddRecognizer(meeting_transcriber);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_leave_meeting(SPXRECOHANDLE hreco)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hreco == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto transcribers = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        auto meeting_transcriber = (*transcribers)[hreco];

        auto cts = SpxQueryInterface<ISpxMeetingTranscriber>(meeting_transcriber);
        // leave meeting, set site to null
        cts->LeaveMeeting();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
