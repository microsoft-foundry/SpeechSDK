//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "speech_synthesis_metadata_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxVisemeEventArgs :
    public ISpxVisemeEventArgs,
    public ISpxVisemeEventArgsInit,
    public CSpxSpeechSynthesisMetadataEventArgs
{
public:

    CSpxVisemeEventArgs() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxVisemeEventArgs)
        SPX_INTERFACE_MAP_ENTRY(ISpxVisemeEventArgsInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechSynthesisMetadataEventArgs)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechSynthesisMetadataEventArgsInit)
    SPX_INTERFACE_MAP_END()

    // --- ISpxVisemeEventArgs ---
    uint64_t GetAudioOffset() override;
    uint32_t GetVisemeId() override;
    std::string& GetAnimation() override;

    // --- ISpxVisemeEventArgsInit ---
    void Init(uint64_t audioOffset, uint32_t visemeId, std::string animation) override;


private:

    DISABLE_COPY_AND_MOVE(CSpxVisemeEventArgs);

    uint64_t m_audioOffset { 0 };
    uint32_t m_visemeId { 0 };
    std::string m_animation;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
