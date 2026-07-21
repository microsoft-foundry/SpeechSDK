//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_hybrid_speech_config.cpp: Definitions for HybridSpeechConfig related C methods
//

#include "stdafx.h"
#include "speechapi_c_hybrid_speech_config.h"
#include "handle_helpers.h"
#include "memory_utils.h"


using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

std::shared_ptr<ISpxSpeechConfig> speech_config_from_handle(SPXSPEECHCONFIGHANDLE handle)
{
    return speech_config_is_handle_valid(handle) ? CSpxSharedPtrHandleTableManager::GetPtr<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>(handle) : nullptr;
}

SPXAPI hybrid_speech_config_create(SPXSPEECHCONFIGHANDLE* hconfig, SPXSPEECHCONFIGHANDLE hcloudSpeechConfig, SPXSPEECHCONFIGHANDLE hembeddedSpeechConfig)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hcloudSpeechConfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hembeddedSpeechConfig));

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hconfig = SPXHANDLE_INVALID;

        // Note, the use of ISpxSpeechConfig and CSpxEmbeddedSpeechConfig here is intentional.
        auto config = SpxCreateObjectWithSite<ISpxSpeechConfig>("CSpxEmbeddedSpeechConfig", SpxGetRootSite());
        SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE, config == nullptr);

        auto config_property_bag = SpxQueryInterface<ISpxNamedProperties>(config);

        // Copy properties of cloud speech config

        auto cloud_config = speech_config_from_handle(hcloudSpeechConfig);

        if (cloud_config != nullptr)
        {
            Memory::CheckObjectCount(hcloudSpeechConfig);

            auto cloud_config_property_bag = SpxQueryInterface<ISpxNamedProperties>(cloud_config);
            if (cloud_config_property_bag != nullptr)
            {
                config_property_bag->Copy(cloud_config_property_bag, false);
            }
        }

        // Copy properties of embedded speech config

        auto embedded_config = speech_config_from_handle(hembeddedSpeechConfig);

        if (embedded_config != nullptr)
        {
            Memory::CheckObjectCount(hembeddedSpeechConfig);

            auto embedded_config_property_bag = SpxQueryInterface<ISpxNamedProperties>(embedded_config);
            if (embedded_config_property_bag != nullptr)
            {
                config_property_bag->Copy(embedded_config_property_bag, false);
            }

            // Set offline model search paths in hybrid speech config

            auto sourceEmbeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(embedded_config);
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, sourceEmbeddedConfig == nullptr);

            auto searchPathList = sourceEmbeddedConfig->GetSearchPathList();
            if (!searchPathList.empty())
            {
                auto targetEmbeddedConfig = SpxQueryInterface<ISpxEmbeddedSpeechConfig>(config);
                SPX_THROW_HR_IF(SPXERR_INVALID_ARG, targetEmbeddedConfig == nullptr);

                for (auto& path : PAL::split(searchPathList, ';'))
                {
                    targetEmbeddedConfig->AddSearchPath(path.c_str());
                }
            }
        }

        // Set recognition backend
        config_property_bag->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_RecoBackend), "hybrid");
        // Set synthesis backend
        config_property_bag->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_SynthBackend), "hybrid");
        config_property_bag->SetStringValue("SPEECH-SynthBackendSwitchingPolicy", "auto");

        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        *hconfig = confighandles->TrackHandle(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
