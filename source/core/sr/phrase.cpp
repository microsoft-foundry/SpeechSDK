//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "phrase.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

void CSpxPhrase::InitPhrase(const wchar_t* phrase)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_phrase.empty());
    m_phrase = phrase;
}

std::wstring CSpxPhrase::GetPhrase() const
{
    return m_phrase;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
