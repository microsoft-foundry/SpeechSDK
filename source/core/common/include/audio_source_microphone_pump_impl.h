//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_source_microphone_pump_impl.h: Implementation declarations for ISpxAudioSourceMicrophonePumpImpl
//

#pragma once
#include "spxcore_common.h"
#include "site_helpers.h"
#include "service_helpers.h"
#include "audio_source_init_not_impl.h"
#include "audio_source_control_adapts_audio_pump_impl.h"
#include "audio_source_simple_impl.h"
#include "audio_processing_options.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class T>
class ISpxAudioSourceMicrophonePumpImpl :
    public ISpxAudioSourceInitNotImpl,
    public ISpxAudioSourceControlAdaptsAudioPumpImpl<T>
{
    public:

    ISpxAudioSourceMicrophonePumpImpl() = default;

    void InitFromMicrophone() final
    {
        std::shared_ptr<ISpxNamedProperties> properties = SpxQueryService<ISpxNamedProperties>(ISpxInterfaceBase::shared_from_this());
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, properties == nullptr);

        bool useMasAudioPump = false;

        // For conversation transcription, we use MAS audio pump by default. MAS audio pump will get the multi-channel raw audio from the
        // microphone, which later on is passed to transcription service.
        bool isConversationTranscription = properties->GetOr<bool>("ConversationTranscriptionInRoomAndOnline", false);
        auto audioProcessingOptionsJson = properties->GetOr(PropertyId::AudioConfig_AudioProcessingOptions, "");
        if (isConversationTranscription)
        {
            // If client has not specified audio processing options for conversation transcription, then the default is assumed to be mono audio.
            // MAS is currenlty only used with the 7 + reference channel format and NOT for mono audio.
            if (!audioProcessingOptionsJson.empty())
            {
                auto audioProcessingOptions = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", this);
                audioProcessingOptions->InitFromJson(audioProcessingOptionsJson);
                if (audioProcessingOptions->GetMicrophoneCount() != 1)
                {
                    // With conversation transcription, MAS is currently used for other than single channel audio.
                    useMasAudioPump = true;
                }
            }
        }

        // For scenarios other than conversation transcription, we check whether AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT, AUDIO_INPUT_PROCESSING_ENABLE_V2,
        // or AUDIO_INPUT_PROCESSING_PNS_ENABLE is set. If audio processing is enabled, we will use MAS audio pump to get the processed audio from the microphone.
        if (!audioProcessingOptionsJson.empty())
        {
            auto audioProcessingOptions = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", this);
            audioProcessingOptions->InitFromJson(audioProcessingOptionsJson);
            if (((audioProcessingOptions->GetAudioProcessingFlags() & AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT) != 0) ||
                ((audioProcessingOptions->GetAudioProcessingFlags() & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0) ||
                ((audioProcessingOptions->GetAudioProcessingFlags() & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0))
            {
                useMasAudioPump = true;
            }
            if ((audioProcessingOptions->GetAudioProcessingFlags() & AUDIO_INPUT_PROCESSING_ENABLE_VOICE_ACTIVITY_DETECTION) != 0
                // Ignore Carbon VAD when embedded SR is used because eSR has an internal VAD
                && properties->GetOr<std::string>(PropertyId::SpeechServiceConnection_RecoBackend, std::string()) != "offline")
            {
                properties->SetStringValue(g_Detection_VadModeOn, "true");
            }
            else
            {
                properties->SetStringValue(g_Detection_VadModeOn, "false");
            }
        }

        ISpxAudioSourceControlAdaptsAudioPumpImpl<T>::InitMicrophonePump(useMasAudioPump ? "CSpxMasAudioPump" : "CSpxInteractiveMicrophone");
    }

protected:
    inline void* QueryInterfaceMicrophonePumpImpl(uint64_t interfaceTypeId)
    {
        return ISpxAudioSourceControlAdaptsAudioPumpImpl<T>::QueryInterfacePumpImpl(interfaceTypeId);
    }

    inline std::shared_ptr<ISpxInterfaceBase> QueryServiceMicrophonePumpImpl(uint64_t serviceTypeId)
    {
        return ISpxAudioSourceControlAdaptsAudioPumpImpl<T>::QueryServicePumpImpl(serviceTypeId);
    }
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
