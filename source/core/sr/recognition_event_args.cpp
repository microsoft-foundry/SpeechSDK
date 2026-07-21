//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "recognition_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxRecognitionEventArgs::CSpxRecognitionEventArgs()
{
    m_offset = 0;
}

const std::wstring& CSpxRecognitionEventArgs::GetSessionId()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_sessionId.length() == 0);
    return m_sessionId;
}

const uint64_t& CSpxRecognitionEventArgs::GetOffset()
{
    return m_offset;
}

std::shared_ptr<ISpxRecognitionResult> CSpxRecognitionEventArgs::GetResult()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_result.get() == nullptr);
    return m_result;
}

void CSpxRecognitionEventArgs::Init(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_sessionId.length() != 0);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_result.get() != nullptr);

    m_sessionId = sessionId;
    m_result = result;
    m_offset = m_result->GetOffset();
}

void CSpxRecognitionEventArgs::Init(const std::wstring& sessionId, uint64_t offset)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_sessionId.length() != 0);

    m_sessionId = sessionId;
    m_offset = offset;
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
