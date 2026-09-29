//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "interface_delegate_helpers.h"
#include "ispxinterfaces.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxAudioProcessor>>
class CSpxAudioProcessorDelegateHelper : public DelegateToHelperT
{
private:

    using I = ISpxAudioProcessor;
    using C = CSpxAudioProcessorDelegateHelper<DelegateToHelperT>;

public:

    SPX_DELEGATE_ACCESSORS(SpxAudioProcessor, DelegateToHelperT, ISpxAudioProcessor)

    void DelegateSetFormat(const SPXWAVEFORMATEX* pformat)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SetFormat, pformat);
    }

    void DelegateProcessAudio(const DataChunkPtr& audioChunk)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::ProcessAudio, audioChunk);
    }

    // Inline commit: forward the commit marker to the wrapped processor.
    // Without this, a pass-through processor built on this helper inherits
    // ISpxAudioProcessor's default no-op ProcessCommit and the commit is
    // silently dropped instead of reaching the session.
    void DelegateProcessCommit(uint32_t token, uint64_t offsetBytes, bool hasChannel, uint32_t channelId)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::ProcessCommit, token, offsetBytes, hasChannel, channelId);
    }
};
}}}} // Microsoft::CognitiveServices::Speech::Impl
