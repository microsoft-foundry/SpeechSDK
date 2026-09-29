//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "reco_engine_adapter_delegate_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <typename DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxAudioProcessor>>
class ISpxAudioProcessorDelegateImpl :
    public CSpxAudioProcessorDelegateHelper<DelegateToHelperT>,
    public ISpxAudioProcessor
{
private:

    using D = CSpxAudioProcessorDelegateHelper<DelegateToHelperT>;

public:
    void SetFormat(const SPXWAVEFORMATEX* pformat) override
    {
        D::DelegateSetFormat(pformat);
    }

    void ProcessAudio(const DataChunkPtr& audioChunk) override
    {
        D::DelegateProcessAudio(audioChunk);
    }

    // Inline commit: forward the commit marker on to the wrapped processor,
    // so a pass-through processor does not silently drop it.
    void ProcessCommit(uint32_t token, uint64_t offsetBytes, bool hasChannel, uint32_t channelId) override
    {
        D::DelegateProcessCommit(token, offsetBytes, hasChannel, channelId);
    }
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
