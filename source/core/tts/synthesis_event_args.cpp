//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// synthesis_event_args.cpp: Implementation definitions for CSpxSynthesisEventArgs C++ class
//

#include "stdafx.h"
#include "synthesis_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxSynthesisEventArgs::CSpxSynthesisEventArgs()
{
}

std::shared_ptr<ISpxSynthesisResult> CSpxSynthesisEventArgs::GetResult()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_result == nullptr);
    return m_result;
}

void CSpxSynthesisEventArgs::Init(std::shared_ptr<ISpxSynthesisResult> result)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_result != nullptr);
    m_result = result;
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
