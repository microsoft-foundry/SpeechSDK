//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speech_synthesis_metadata_event_args.h: Implementation for CSpxSpeechSynthesisMetadataEventArgs C++ class
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxSpeechSynthesisMetadataEventArgs :
    public ISpxSpeechSynthesisMetadataEventArgs,
    public ISpxSpeechSynthesisMetadataEventArgsInit
{
public:

    CSpxSpeechSynthesisMetadataEventArgs() { }

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechSynthesisMetadataEventArgs)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechSynthesisMetadataEventArgsInit)
    SPX_INTERFACE_MAP_END()

    // --- ISpxSpeechSynthesisMetadataEventArgs
    std::string& GetResultId() override { return m_resultId; }

    // --- ISpxSpeechSynthesisMetadataEventArgsInit
    void SetResultId(std::string resultId) override { m_resultId = std::move(resultId); }

protected:
    std::string m_resultId;

private:

    CSpxSpeechSynthesisMetadataEventArgs(const CSpxSpeechSynthesisMetadataEventArgs&) = delete;
    CSpxSpeechSynthesisMetadataEventArgs(const CSpxSpeechSynthesisMetadataEventArgs&&) = delete;

    CSpxSpeechSynthesisMetadataEventArgs& operator=(const CSpxSpeechSynthesisMetadataEventArgs&) = delete;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
