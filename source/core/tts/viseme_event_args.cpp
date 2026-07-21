//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "viseme_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

uint64_t CSpxVisemeEventArgs::GetAudioOffset()
{
    return m_audioOffset;
}

uint32_t CSpxVisemeEventArgs::GetVisemeId()
{
    return m_visemeId;
}

std::string& CSpxVisemeEventArgs::GetAnimation()
{
    return m_animation;
}

void CSpxVisemeEventArgs::Init(uint64_t audioOffset, uint32_t visemeId, std::string animation)
{
    m_audioOffset = audioOffset;
    m_visemeId = visemeId;
    m_animation = std::move(animation);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
