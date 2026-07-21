//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// word_boundary_event_args.cpp: Implementation definitions for CSpxWordBoundaryEventArgs C++ class
//

#include "stdafx.h"
#include "word_boundary_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxWordBoundaryEventArgs::CSpxWordBoundaryEventArgs()
{
}

uint64_t CSpxWordBoundaryEventArgs::GetAudioOffset()
{
    return m_audioOffset;
}

uint64_t CSpxWordBoundaryEventArgs::GetDuration()
{
    return m_duration;
}

uint32_t CSpxWordBoundaryEventArgs::GetTextOffset()
{
    return m_textOffset;
}

uint32_t CSpxWordBoundaryEventArgs::GetWordLength()
{
    return m_wordLength;
}

std::string& CSpxWordBoundaryEventArgs::GetText()
{
    return m_text;
}

SpeechSynthesisBoundaryType CSpxWordBoundaryEventArgs::GetBoundaryType()
{
    return m_boundaryType;
}

void CSpxWordBoundaryEventArgs::Init(uint64_t audioOffset, uint64_t duration, uint32_t textOffset,
                                     uint32_t wordLength, std::string text, SpeechSynthesisBoundaryType boundaryType)
{
    m_audioOffset = audioOffset;
    m_duration = duration;
    m_textOffset = textOffset;
    m_wordLength = wordLength;
    m_text = std::move(text);
    m_boundaryType = boundaryType;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
