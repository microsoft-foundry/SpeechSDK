//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
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
class ISpxAudioSourceStreamPumpImpl :
    public ISpxAudioSourceInitNotImpl,
    public ISpxAudioSourceControlAdaptsAudioPumpImpl<T>
{
    public:

    ISpxAudioSourceStreamPumpImpl() = default;

    void InitFromStream(std::shared_ptr<ISpxAudioStream> stream) final
    {
        // All the processing for conversation transcription happens on the cloud. So, we do not enable on-device audio processing for
        // conversation transcription. Audio is read from the stream and is passed on to the transcription service.
        // For other scenarios, we enable MAS's audio processing if AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT, AUDIO_INPUT_PROCESSING_ENABLE_V2,
        // or AUDIO_INPUT_PROCESSING_PNS_ENABLE flag is set in the audio processing options. When MAS is enabled, audio is read from the
        // stream, passed to MAS processor for audio processing and then the processed audio is passed to the recognizer.
        std::shared_ptr<ISpxNamedProperties> properties = SpxQueryService<ISpxNamedProperties>(ISpxInterfaceBase::shared_from_this());
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, properties == nullptr);
        bool isConversationTranscription = properties->GetOr<bool>("ConversationTranscriptionInRoomAndOnline", false);
        if (!isConversationTranscription)
        {
            auto audioProcessingOptionsJson = properties->GetOr(PropertyId::AudioConfig_AudioProcessingOptions, "");
            if (!audioProcessingOptionsJson.empty())
            {
                auto audioProcessingOptions = SpxCreateObjectWithSite<ISpxAudioProcessingOptions>("CSpxAudioProcessingOptions", this);
                audioProcessingOptions->InitFromJson(audioProcessingOptionsJson);
                if (((audioProcessingOptions->GetAudioProcessingFlags() & AUDIO_INPUT_PROCESSING_ENABLE_DEFAULT) != 0) ||
                    ((audioProcessingOptions->GetAudioProcessingFlags() & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0) ||
                    ((audioProcessingOptions->GetAudioProcessingFlags() & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0))
                {
                    properties->SetStringValue("enableMasAudioInputProcessing", "true");
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
        }
        ISpxAudioSourceControlAdaptsAudioPumpImpl<T>::InitAudioStreamPump("CSpxAudioPump", stream);
    }

protected:
    inline void* QueryInterfaceStreamPumpImpl(uint64_t interfaceTypeId)
    {
        return ISpxAudioSourceControlAdaptsAudioPumpImpl<T>::QueryInterfacePumpImpl(interfaceTypeId);
    }

    inline std::shared_ptr<ISpxInterfaceBase> QueryServiceStreamPumpImpl(uint64_t serviceTypeId)
    {
        return ISpxAudioSourceControlAdaptsAudioPumpImpl<T>::QueryServicePumpImpl(serviceTypeId);
    }
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
