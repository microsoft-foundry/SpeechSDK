//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// connection_event_args.h: Implementation declarations for CSpxConnectionEventArgs C++ class
//

#include "stdafx.h"
#include "connection_event_args.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxConnectionEventArgs::CSpxConnectionEventArgs(): m_initialized(false)
{
}

const std::wstring& CSpxConnectionEventArgs::GetSessionId()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !m_initialized);
    return m_sessionId;
}

void CSpxConnectionEventArgs::Init(const std::wstring& sessionId)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_initialized);
    m_initialized = true;
    m_sessionId = sessionId;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
