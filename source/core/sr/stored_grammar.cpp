//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "stored_grammar.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

void CSpxStoredGrammar::InitStoredGrammar(const wchar_t* id)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_id.empty());
    m_id = PAL::ToString(id);
}

std::list<std::string> CSpxStoredGrammar::GetListenForList()
{
    auto retVal = std::list<std::string>();
    retVal.push_front(m_id);

    return retVal;
}

}}}} // Microsoft::CognitiveServices::Speech::Impl
