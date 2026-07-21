//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// interactive_microphone.cpp: Implementation definitions for CSpxInteractiveMicrophone C++ class
//

#include "stdafx.h"
#include "interactive_microphone.h"
#include "site_helpers.h"
#include "create_object_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxInteractiveMicrophone::CSpxInteractiveMicrophone() :
    ISpxDelegateAudioPumpImpl()
{
}

void CSpxInteractiveMicrophone::Init()
{
    if (m_delegateToAudioPump == nullptr)
    {
        auto site = GetSite();
        m_delegateToAudioPump = SpxCreateObjectWithSite<ISpxAudioPump>("CSpxMicrophonePump", site);
        SPX_THROW_HR_IF(SPXERR_AUDIO_SYS_LIBRARY_NOT_FOUND, m_delegateToAudioPump == nullptr);
    }
}

void CSpxInteractiveMicrophone::Term()
{
    SpxTermAndClear(m_delegateToAudioPump);
}

// relay the property query to its site, which is CSpxAudioStreamSession
std::shared_ptr<ISpxNamedProperties> CSpxInteractiveMicrophone::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
