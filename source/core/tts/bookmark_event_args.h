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


class CSpxBookmarkEventArgs :
    public ISpxBookmarkEventArgs,
    public ISpxBookmarkEventArgsInit,
    public CSpxSpeechSynthesisMetadataEventArgs
{
public:

    CSpxBookmarkEventArgs() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxBookmarkEventArgs)
        SPX_INTERFACE_MAP_ENTRY(ISpxBookmarkEventArgsInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechSynthesisMetadataEventArgs)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechSynthesisMetadataEventArgsInit)
    SPX_INTERFACE_MAP_END()

    // --- ISpxBookmarkEventArgs ---
    uint64_t GetAudioOffset() override;
    std::string& GetText() override;

    // --- ISpxBookmarkEventArgsInit ---
    void Init(uint64_t audioOffset, std::string text) override;


private:

    DISABLE_COPY_AND_MOVE(CSpxBookmarkEventArgs);

    uint64_t m_audioOffset { 0 };
    std::string m_text;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
