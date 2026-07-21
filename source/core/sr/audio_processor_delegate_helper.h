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
        return InvokeOnDelegateR(C::GetDelegate(), &I::SetFormat, pformat);
    }

    void DelegateProcessAudio(const DataChunkPtr& audioChunk)
    {
        return InvokeOnDelegateR(C::GetDelegate(), &I::ProcessAudio, audioChunk);
    }
};
}}}} // Microsoft::CognitiveServices::Speech::Impl
