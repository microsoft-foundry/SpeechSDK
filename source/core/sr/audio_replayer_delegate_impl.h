//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "audio_replayer_delegate_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <typename DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxAudioReplayer>>
class ISpxAudioReplayerDelegateImpl :
    public CSpxAudioReplayerDelegateHelper<DelegateToHelperT>,
    public ISpxAudioReplayer
{
private:

    using D = CSpxAudioReplayerDelegateHelper<DelegateToHelperT>;

public:
    void ShrinkReplayBuffer(uint64_t newBaseOffset) override
    {
        D::DelegateShrinkReplayBuffer(newBaseOffset);
    }

    void GetCurrentAudioBufferOffset(uint64_t* offsetInTicks, uint64_t* offsetInBytes) override
    {
        D::DelegateGetCurrentAudioBufferOffset(offsetInTicks, offsetInBytes);
    }

    void GetCurrentAudioContinuationOffset(uint64_t* offsetInTicks) override
    {
        D::DelegateGetCurrentAudioContinuationOffset(offsetInTicks);
    }

    void GetMultiChannelProcessingMode(bool* useMultiChannelProcessing) override
    {
        D::DelegateGetMultiChannelProcessingMode(useMultiChannelProcessing);
    }
};

template <class T>
class ISpxAudioReplayerSiteDelegateToSiteImpl : public ISpxAudioReplayerDelegateImpl<CSpxDelegateToSiteWeakPtrHelper<ISpxAudioReplayer, T>>
{
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
