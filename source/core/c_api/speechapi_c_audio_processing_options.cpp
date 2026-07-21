//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_audio_processing_options.cpp: Public API definitions for audio processing options related C methods and types

#include "stdafx.h"
#include "create_object_helpers.h"
#include "event_helpers.h"
#include "handle_helpers.h"
#include "platform.h"
#include "site_helpers.h"
#include "speechapi_cxx_audio_processing_options.h"

using namespace Microsoft::CognitiveServices::Speech::Audio;
using namespace Microsoft::CognitiveServices::Speech::Impl;

static_assert((int)PresetMicrophoneArrayGeometry::Uninitialized == (int)AudioProcessingOptions_PresetMicrophoneArrayGeometry_Uninitialized, "PresetMicrophoneArrayGeometry should match between C and C++ layers");
static_assert((int)PresetMicrophoneArrayGeometry::Circular7 == (int)AudioProcessingOptions_PresetMicrophoneArrayGeometry_Circular7, "PresetMicrophoneArrayGeometry should match between C and C++ layers");
static_assert((int)PresetMicrophoneArrayGeometry::Circular4 == (int)AudioProcessingOptions_PresetMicrophoneArrayGeometry_Circular4, "PresetMicrophoneArrayGeometry should match between C and C++ layers");
static_assert((int)PresetMicrophoneArrayGeometry::Linear4 == (int)AudioProcessingOptions_PresetMicrophoneArrayGeometry_Linear4, "PresetMicrophoneArrayGeometry should match between C and C++ layers");
static_assert((int)PresetMicrophoneArrayGeometry::Linear2 == (int)AudioProcessingOptions_PresetMicrophoneArrayGeometry_Linear2, "PresetMicrophoneArrayGeometry should match between C and C++ layers");
static_assert((int)PresetMicrophoneArrayGeometry::Mono == (int)AudioProcessingOptions_PresetMicrophoneArrayGeometry_Mono, "PresetMicrophoneArrayGeometry should match between C and C++ layers");
static_assert((int)PresetMicrophoneArrayGeometry::Custom == (int)AudioProcessingOptions_PresetMicrophoneArrayGeometry_Custom, "PresetMicrophoneArrayGeometry should match between C and C++ layers");

static_assert((int)MicrophoneArrayType::Linear == (int)AudioProcessingOptions_MicrophoneArrayType_Linear, "MicrophoneArrayType should match between C and C++ layers");
static_assert((int)MicrophoneArrayType::Planar == (int)AudioProcessingOptions_MicrophoneArrayType_Planar, "MicrophoneArrayType should match between C and C++ layers");

static_assert((int)SpeakerReferenceChannel::None == (int)AudioProcessingOptions_SpeakerReferenceChannel_None, "SpeakerReferenceChannel should match between C and C++ layers");
static_assert((int)SpeakerReferenceChannel::LastChannel == (int)AudioProcessingOptions_SpeakerReferenceChannel_LastChannel, "SpeakerReferenceChannel should match between C and C++ layers");

static inline bool AreAudioProcessingFlagsValid(int audioProcessingFlags)
{
    const bool isPnsEnabled = (audioProcessingFlags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0;
#if defined(_WIN32) && (defined(_M_AMD64) || defined(_M_ARM64))
    // AUDIO_INPUT_PROCESSING_ENABLE_V2 is mutually exclusive with AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT and all the disable flags.
    if (((audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0) &&
        (((audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT) != 0) ||
         ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_DEREVERBERATION) != 0) ||
         ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_NOISE_SUPPRESSION) != 0) ||
         ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_GAIN_CONTROL) != 0) ||
         ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_ECHO_CANCELLATION) != 0)))
    {
        return false;
    }
#else
    // AUDIO_INPUT_PROCESSING_ENABLE_V2 is only supported on Windows x64 and ARM64 platforms.
    if ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0)
    {
        return false;
    }
#endif
    // AUDIO_INPUT_PROCESSING_DISABLE_* flags cannot be set if AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT is not set.
    if (((audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT) == 0) &&
        (((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_DEREVERBERATION) != 0) ||
         ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_NOISE_SUPPRESSION) != 0) ||
         ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_GAIN_CONTROL) != 0) ||
         ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_ECHO_CANCELLATION) != 0)))
    {
        return false;
    }

    // PNS is mutually exclusive with other pipelines and disables all disable flags.
    if (isPnsEnabled && (((audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0) ||
        ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT) != 0) ||
        ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_DEREVERBERATION) != 0) ||
        ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_NOISE_SUPPRESSION) != 0) ||
        ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_GAIN_CONTROL) != 0) ||
        ((audioProcessingFlags & AUDIO_INPUT_PROCESSING_DISABLE_ECHO_CANCELLATION) != 0)))
    {
        return false;
    }
    return true;
}

SPXAPI_(bool) audio_processing_options_is_handle_valid(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions)
{
    return CSpxApiManager::IsValid<SPXAUDIOPROCESSINGOPTIONSHANDLE, ISpxAudioProcessingOptions>(hoptions);
}

SPXAPI audio_processing_options_create(SPXAUDIOPROCESSINGOPTIONSHANDLE* hoptions, int audioProcessingFlags)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hoptions == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        if (!AreAudioProcessingFlagsValid(audioProcessingFlags))
        {
            SPX_TRACE_ERROR("Invalid combination of audio processing flags provided.");
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }

        *hoptions = SPXHANDLE_INVALID;
        auto options = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", SpxGetRootSite());
        options->InitWithProcessingFlags(audioProcessingFlags);
        *hoptions = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioProcessingOptions, SPXAUDIOPROCESSINGOPTIONSHANDLE>(options);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_create_from_preset_microphone_array_geometry(SPXAUDIOPROCESSINGOPTIONSHANDLE* hoptions, int audioProcessingFlags, AudioProcessingOptions_PresetMicrophoneArrayGeometry microphoneArrayGeometry, AudioProcessingOptions_SpeakerReferenceChannel speakerReferenceChannel)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hoptions == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        if (!AreAudioProcessingFlagsValid(audioProcessingFlags))
        {
            SPX_TRACE_ERROR("Invalid combination of audio processing flags provided.");
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }

        *hoptions = SPXHANDLE_INVALID;
        auto options = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", SpxGetRootSite());
        options->InitWithPresetMicrophoneArrayGeometry(audioProcessingFlags, (PresetMicrophoneArrayGeometry)microphoneArrayGeometry, (SpeakerReferenceChannel)speakerReferenceChannel);
        *hoptions = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioProcessingOptions, SPXAUDIOPROCESSINGOPTIONSHANDLE>(options);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_create_from_microphone_array_geometry(SPXAUDIOPROCESSINGOPTIONSHANDLE* hoptions, int audioProcessingFlags, const AudioProcessingOptions_MicrophoneArrayGeometry* microphoneArrayGeometry, AudioProcessingOptions_SpeakerReferenceChannel speakerReferenceChannel)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hoptions == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, microphoneArrayGeometry == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, microphoneArrayGeometry->numberOfMicrophones == 0);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, microphoneArrayGeometry->microphoneCoordinates == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        if (!AreAudioProcessingFlagsValid(audioProcessingFlags))
        {
            SPX_TRACE_ERROR("Invalid combination of audio processing flags provided.");
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }

        std::vector<MicrophoneCoordinates> microphoneCoordinates(microphoneArrayGeometry->microphoneCoordinates, microphoneArrayGeometry->microphoneCoordinates + microphoneArrayGeometry->numberOfMicrophones);
        MicrophoneArrayGeometry geometry((MicrophoneArrayType)microphoneArrayGeometry->microphoneArrayType, microphoneArrayGeometry->beamformingStartAngle, microphoneArrayGeometry->beamformingEndAngle, microphoneCoordinates);

        *hoptions = SPXHANDLE_INVALID;
        auto options = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", SpxGetRootSite());
        options->InitWithMicrophoneArrayGeometry(audioProcessingFlags, geometry, (SpeakerReferenceChannel)speakerReferenceChannel);
        *hoptions = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxAudioProcessingOptions, SPXAUDIOPROCESSINGOPTIONSHANDLE>(options);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_audio_processing_flags(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, int* audioProcessingFlags)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, audioProcessingFlags == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        *audioProcessingFlags = options->GetAudioProcessingFlags();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_preset_microphone_array_geometry(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, AudioProcessingOptions_PresetMicrophoneArrayGeometry* microphoneArrayGeometry)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, microphoneArrayGeometry == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        *microphoneArrayGeometry = static_cast<AudioProcessingOptions_PresetMicrophoneArrayGeometry>(options->GetPresetMicrophoneArrayGeometry());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_microphone_array_type(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, AudioProcessingOptions_MicrophoneArrayType* microphoneArrayType)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, microphoneArrayType == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        *microphoneArrayType = static_cast<AudioProcessingOptions_MicrophoneArrayType>(options->GetMicrophoneArrayType());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_beamforming_start_angle(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, uint16_t* beamformingStartAngle)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, beamformingStartAngle == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        *beamformingStartAngle = options->GetBeamformingStartAngle();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_beamforming_end_angle(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, uint16_t* beamformingEndAngle)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, beamformingEndAngle == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        *beamformingEndAngle = options->GetBeamformingEndAngle();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_microphone_count(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, uint16_t* microphoneCount)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, microphoneCount == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        *microphoneCount = options->GetMicrophoneCount();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_microphone_coordinates(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, AudioProcessingOptions_MicrophoneCoordinates* microphoneCoordinates, uint16_t microphoneCount)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, microphoneCoordinates == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, microphoneCount != options->GetMicrophoneCount());
        std::vector<MicrophoneCoordinates> coordinates = options->GetMicrophoneCoordinates();
        memcpy(microphoneCoordinates, coordinates.data(), microphoneCount * sizeof(MicrophoneCoordinates));
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_speaker_reference_channel(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, AudioProcessingOptions_SpeakerReferenceChannel* speakerReferenceChannel)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, speakerReferenceChannel == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        *speakerReferenceChannel = static_cast<AudioProcessingOptions_SpeakerReferenceChannel>(options->GetSpeakerReferenceChannel());
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_set_speaker_signature(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, const float* speakerSignature, uint32_t length)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, speakerSignature == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, length != 128U);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        std::vector<float> signature(speakerSignature, speakerSignature + length);
        options->SetSpeakerSignature(signature);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_get_speaker_signature(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, float* speakerSignature, uint32_t length, uint32_t* lengthWritten)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !audio_processing_options_is_handle_valid(hoptions));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, lengthWritten == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto options = SpxGetPtrFromHandle<ISpxAudioProcessingOptions>(hoptions);
        auto signature = options->GetSpeakerSignature();
        *lengthWritten = static_cast<uint32_t>(signature.size());
        if (speakerSignature != nullptr)
        {
            if (signature.empty())
            {
                // Zero out the buffer when no signature is available
                memset(speakerSignature, 0, length * sizeof(float));
            }
            else
            {
                SPX_THROW_HR_IF(SPXERR_INVALID_ARG, length < signature.size());
                memcpy(speakerSignature, signature.data(), signature.size() * sizeof(float));
            }
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_processing_options_release(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXAUDIOPROCESSINGOPTIONSHANDLE, ISpxAudioProcessingOptions>(hoptions);
}

SPXAPI audio_processing_options_get_property_bag(SPXAUDIOPROCESSINGOPTIONSHANDLE hoptions, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXAUDIOPROCESSINGOPTIONSHANDLE, ISpxAudioProcessingOptions, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hoptions, hpropbag);
}
