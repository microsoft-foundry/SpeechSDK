//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "interface_delegate_helpers.h"
#include "ispxinterfaces.h"
#include "audio_processor_delegate_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxAudioReplayer>>
class CSpxAudioReplayerDelegateHelper :
    public DelegateToHelperT
{
private:

    using I = ISpxAudioReplayer;
    using C = CSpxAudioReplayerDelegateHelper<DelegateToHelperT>;

public:

    SPX_DELEGATE_ACCESSORS(AudioReplayer, DelegateToHelperT, ISpxAudioReplayer)

    void DelegateShrinkReplayBuffer(uint64_t newBaseOffset)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::ShrinkReplayBuffer, newBaseOffset);
    }

    void DelegateGetCurrentAudioBufferOffset(uint64_t* offsetInTicks, uint64_t* offsetInBytes)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::GetCurrentAudioBufferOffset, offsetInTicks, offsetInBytes);
    }

    void DelegateGetCurrentAudioContinuationOffset(uint64_t* offsetInTicks)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::GetCurrentAudioContinuationOffset, offsetInTicks);
    }

    void DelegateGetMultiChannelProcessingMode(bool* useMultiChannelProcessing)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::GetMultiChannelProcessingMode, useMultiChannelProcessing);
    }
};
}}}} // Microsoft::CognitiveServices::Speech::Impl
