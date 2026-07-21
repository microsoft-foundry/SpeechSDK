//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_processing_options.h: Implementation declarations for CSpxAudioProcessingOptions C++ class
//

#pragma once
#include "interface_helpers.h"
#include "service_helpers.h"
#include "property_bag_impl.h"
#include "speechapi_cxx_enums.h"
#include "speechapi_cxx_audio_processing_options.h"
#include <speechapi_cxx_common.h>
#include <object_with_site_init_impl.h>
#include <unordered_map>

using namespace Microsoft::CognitiveServices::Speech::Audio;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxAudioProcessingOptions :
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxGenericSite,
    public ISpxServiceProvider,
    public ISpxPropertyBagImpl,
    public ISpxAudioProcessingOptions
{
public:

    CSpxAudioProcessingOptions();
    virtual ~CSpxAudioProcessingOptions();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessingOptions)
    SPX_INTERFACE_MAP_END()

    // --- IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

    // --- ISpxAudioProcessingOptions
    void InitWithProcessingFlags(int audioProcessingFlags) override;
    void InitWithPresetMicrophoneArrayGeometry(int audioProcessingFlags, PresetMicrophoneArrayGeometry microphoneArrayGeometry, SpeakerReferenceChannel speakerReferenceChannel) override;
    void InitWithMicrophoneArrayGeometry(int audioProcessingFlags, MicrophoneArrayGeometry microphoneArrayGeometry, SpeakerReferenceChannel speakerReferenceChannel) override;
    void InitFromJson(const std::string& audioProcessingOptionsJson) override;

    inline int GetAudioProcessingFlags() override;
    inline Audio::PresetMicrophoneArrayGeometry GetPresetMicrophoneArrayGeometry() override;
    inline Audio::MicrophoneArrayType GetMicrophoneArrayType() override;
    inline uint16_t GetBeamformingStartAngle() override;
    inline uint16_t GetBeamformingEndAngle() override;
    inline uint16_t GetMicrophoneCount() override;
    inline std::vector<Audio::MicrophoneCoordinates> GetMicrophoneCoordinates() override;
    inline SpeakerReferenceChannel GetSpeakerReferenceChannel() override;
    inline std::string GetModelPath(ModelType modelType) override;
    inline void SetSpeakerSignature(const std::vector<float>& signature) override;
    inline std::vector<float> GetSpeakerSignature() override;

    std::string ToJson() override;

private:

    DISABLE_COPY_AND_MOVE(CSpxAudioProcessingOptions);

    std::vector<MicrophoneCoordinates> ConvertPresetGeometryToCoordinates(PresetMicrophoneArrayGeometry microphoneArrayGeometry);
    void InitModelPaths();

    int m_audioProcessingFlags;
    PresetMicrophoneArrayGeometry m_microphoneArrayGeometry;
    MicrophoneArrayType m_microphoneArrayType;
    uint16_t m_beamformingStartAngle;
    uint16_t m_beamformingEndAngle;
    std::vector<MicrophoneCoordinates> m_microphoneCoordinates;
    SpeakerReferenceChannel m_speakerReferenceChannel;
    std::unordered_map<std::string, std::string> m_modelPaths;
    std::vector<float> m_speakerSignature;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
