//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "bookmark_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

uint64_t CSpxBookmarkEventArgs::GetAudioOffset()
{
    return m_audioOffset;
}

std::string& CSpxBookmarkEventArgs::GetText()
{
    return m_text;
}

void CSpxBookmarkEventArgs::Init(uint64_t audioOffset, std::string text)
{
    m_audioOffset = audioOffset;
    m_text = std::move(text);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
