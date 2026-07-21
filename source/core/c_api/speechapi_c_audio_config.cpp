//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_audio_config.cpp: Public API definitions for audio configuration C methods and types

#include "stdafx.h"
#include "common.h"
#include "create_object_helpers.h"
#include "event_helpers.h"
#include "handle_helpers.h"
#include "platform.h"
#include "site_helpers.h"
#include "string_utils.h"
#include <assert.h>
#include "property_id_2_name_map.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Audio;
using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI_(bool) audio_config_is_handle_valid(SPXAUDIOCONFIGHANDLE haudioConfig)
{
    return CSpxApiManager::IsValid<SPXAUDIOCONFIGHANDLE, ISpxAudioConfig>(haudioConfig);
}

SPXAPI audio_config_create_audio_input_from_default_microphone(SPXAUDIOCONFIGHANDLE* haudioConfig)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        config->InitFromDefaultDevice();

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_audio_input_from_a_microphone(SPXAUDIOCONFIGHANDLE* haudioConfig, const char* deviceName)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        auto properties = SpxQueryService<ISpxNamedProperties>(config);
        properties->Set(PropertyId::AudioConfig_DeviceNameForCapture, deviceName);

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_audio_input_from_wav_file_name(SPXAUDIOCONFIGHANDLE* haudioConfig, const char* fileName)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, fileName == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        config->InitFromFile(fileName);

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_audio_input_from_stream(SPXAUDIOCONFIGHANDLE* haudioConfig, SPXAUDIOSTREAMHANDLE haudioStream)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioStream == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto stream = CSpxSharedPtrHandleTableManager::GetPtr<ISpxAudioStream, SPXAUDIOSTREAMHANDLE>(haudioStream);
        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        config->InitFromStream(stream);

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_push_audio_input_stream(SPXAUDIOCONFIGHANDLE* haudioConfig, SPXAUDIOSTREAMHANDLE* haudioStream, SPXAUDIOSTREAMFORMATHANDLE hformat)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioStream == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_THROW_ON_FAIL(audio_stream_create_push_audio_input_stream(haudioStream, hformat));
        SPX_THROW_ON_FAIL(audio_config_create_audio_input_from_stream(haudioConfig, *haudioStream));
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_pull_audio_input_stream(SPXAUDIOCONFIGHANDLE* haudioConfig, SPXAUDIOSTREAMHANDLE* haudioStream, SPXAUDIOSTREAMFORMATHANDLE hformat)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioStream == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_THROW_ON_FAIL(audio_stream_create_pull_audio_input_stream(haudioStream, hformat));
        SPX_THROW_ON_FAIL(audio_config_create_audio_input_from_stream(haudioConfig, *haudioStream));
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_audio_output_from_default_speaker(SPXAUDIOCONFIGHANDLE* haudioConfig)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        config->InitFromDefaultDevice();

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_audio_output_from_a_speaker(SPXAUDIOCONFIGHANDLE* haudioConfig, const char* deviceName)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        auto properties = SpxQueryService<ISpxNamedProperties>(config);
        properties->Set(PropertyId::AudioConfig_DeviceNameForRender, deviceName);

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_audio_output_from_wav_file_name(SPXAUDIOCONFIGHANDLE* haudioConfig, const char* fileName)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, fileName == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        config->InitFromFile(fileName);

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_create_audio_output_from_stream(SPXAUDIOCONFIGHANDLE* haudioConfig, SPXAUDIOSTREAMHANDLE haudioStream)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioConfig == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioStream == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *haudioConfig = SPXHANDLE_INVALID;

        auto stream = CSpxSharedPtrHandleTableManager::GetPtr<ISpxAudioStream, SPXAUDIOSTREAMHANDLE>(haudioStream);
        auto config = SpxCreateObjectWithSite<ISpxAudioConfig>("CSpxAudioConfig", SpxGetRootSite());
        config->InitFromStream(stream);

        *haudioConfig = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(config);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_release(SPXAUDIOCONFIGHANDLE haudioConfig)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXAUDIOCONFIGHANDLE, ISpxAudioConfig>(haudioConfig);
}

SPXAPI audio_config_get_property_bag(SPXAUDIOCONFIGHANDLE haudioConfig, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXAUDIOCONFIGHANDLE, ISpxAudioConfig, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(haudioConfig, hpropbag);
}

SPXAPI audio_config_set_audio_processing_options(SPXAUDIOCONFIGHANDLE haudioConfig, SPXAUDIOPROCESSINGOPTIONSHANDLE haudioProcessingOptions)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_config_is_handle_valid(haudioConfig));

    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(haudioProcessingOptions));
        auto audioProcessingOptions = CSpxSharedPtrHandleTableManager::GetPtr<ISpxAudioProcessingOptions, SPXAUDIOPROCESSINGOPTIONSHANDLE>(haudioProcessingOptions);

        auto audioConfig = CSpxSharedPtrHandleTableManager::GetPtr<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(haudioConfig);

        // If audio input is through a file or a stream, check if microphone array geometry is provided.
        const int flags = audioProcessingOptions->GetAudioProcessingFlags();
        bool isAudioProcessingEnabled = ((flags & AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT) != 0) ||
                                        ((flags & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0) ||
                                        ((flags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0);
        if ((!audioConfig->GetFileName().empty() || (audioConfig->GetStream() != nullptr)) &&
            isAudioProcessingEnabled &&
            (audioProcessingOptions->GetPresetMicrophoneArrayGeometry() == PresetMicrophoneArrayGeometry::Uninitialized))
        {
            SPX_TRACE_ERROR("Microphone array geometry must be specified when using a file or a stream as audio input.");
            SPX_THROW_HR(SPXERR_UNINITIALIZED);
        }

        // PNS only supports mono mic and optional mono speaker reference.
        // Allow Uninitialized (treated as mono) to avoid injecting PMA geometry defaults for this mode.
        if ((flags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0)
        {
            auto geom = audioProcessingOptions->GetPresetMicrophoneArrayGeometry();
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, geom != PresetMicrophoneArrayGeometry::Mono && geom != PresetMicrophoneArrayGeometry::Uninitialized);
        }

        auto properties = SpxQueryInterface<ISpxNamedProperties>(audioConfig);
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, properties == nullptr);
        std::string audioProcessingOptionsJson = audioProcessingOptions->ToJson();
        properties->Set(PropertyId::AudioConfig_AudioProcessingOptions, audioProcessingOptionsJson.c_str());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_config_get_audio_processing_options(SPXAUDIOCONFIGHANDLE haudioConfig, SPXAUDIOPROCESSINGOPTIONSHANDLE* haudioProcessingOptions)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_config_is_handle_valid(haudioConfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, haudioProcessingOptions == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto audioConfig = CSpxSharedPtrHandleTableManager::GetPtr<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(haudioConfig);
        auto properties = SpxQueryInterface<ISpxNamedProperties>(audioConfig);
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, properties == nullptr);
        auto audioProcessingOptionsJson = properties->GetOr(PropertyId::AudioConfig_AudioProcessingOptions, "");
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, audioProcessingOptionsJson.empty());

        auto audioProcessingOptions = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", SpxGetRootSite());
        audioProcessingOptions->InitFromJson(audioProcessingOptionsJson);
        *haudioProcessingOptions = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioProcessingOptions, SPXAUDIOPROCESSINGOPTIONSHANDLE>(audioProcessingOptions);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

// FUTURE DEVELOPMENT: The config method group could be extended in the future to support additional scenarios, for example:
//
//    SPXAPI audio_config_create_audio_input_from_url(SPXAUDIOCONFIGHANDLE* haudioConfig, const char* url);
//    SPXAPI audio_config_create_audio_input_from_url_stream(SPXAUDIOCONFIGHANDLE* haudioConfig, const char* url, SPXAUDIOSTREAMFORMATHANDLE hformat);
//    SPXAPI audio_config_create_audio_input_from_file_stream(SPXAUDIOCONFIGHANDLE* haudioConfig, const char* fileName, SPXAUDIOSTREAMFORMATHANDLE hformat);
//    SPXAPI audio_config_create_audio_input_from_device(SPXAUDIOCONFIGHANDLE* haudioConfig, SPXAUDIODEVICEHANDLE* hdevice);
