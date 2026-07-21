//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_source_control_simple_impl.h: Implementation declarations/definitions for ISpxAudioSourceControlSimpleImpl
//

#pragma once
#include "spxcore_common.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class ISpxAudioSourceControlSimpleImpl : public ISpxAudioSourceControl
{
public:

    void StartAudio(std::shared_ptr<ISpxAudioSourceNotifyMe> requestNotify) override
    {
        SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, GetNotifyRequestor() != nullptr);
        SetNotifyRequestor(requestNotify);
    }

    void StopAudio() override
    {
        SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !GetNotifyRequestor());
        SetNotifyRequestor(nullptr);
    }

protected:

    bool IsAudioStarted() { return GetNotifyRequestor() != nullptr; }

    template <typename... Args>
    void NotifyAudioSourceRequestor(Args&&... args)
    {
        if (m_requestNotify != nullptr)
        {
            m_requestNotify->NotifyMe(std::forward<Args>(args)...);
        }
    }

private:

    void SetNotifyRequestor(std::shared_ptr<ISpxAudioSourceNotifyMe> target)
    {
        m_requestNotify = target;
    }

    std::shared_ptr<ISpxAudioSourceNotifyMe> GetNotifyRequestor() { return m_requestNotify; }

    std::shared_ptr<ISpxAudioSourceNotifyMe> m_requestNotify;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
