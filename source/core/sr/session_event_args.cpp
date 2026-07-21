//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "session_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxSessionEventArgs::CSpxSessionEventArgs()
{
}

const std::wstring& CSpxSessionEventArgs::GetSessionId()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_sessionId.length() == 0);
    return m_sessionId;
}

void CSpxSessionEventArgs::Init(const std::wstring& sessionId)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_sessionId.length() != 0);
    m_sessionId = sessionId;
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
