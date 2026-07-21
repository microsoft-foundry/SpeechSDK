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
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
