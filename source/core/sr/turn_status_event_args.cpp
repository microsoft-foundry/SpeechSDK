//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// turn_status_event_args.cpp: Implementation definitions for CSpxTurnStatusEventArgs C++ class.
//
#include "stdafx.h"
#include "http_status_codes.h"
#include "turn_status_event_args.h"

using namespace Microsoft::CognitiveServices::Speech::USP;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxTurnStatusEventArgs::CSpxTurnStatusEventArgs()
    : m_status((int)HttpStatusCode::NOT_FOUND)
{
}

const std::string& CSpxTurnStatusEventArgs::GetInteractionId() const
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_interactionId.empty());
    return m_interactionId;
}

const std::string& CSpxTurnStatusEventArgs::GetConversationId() const
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_conversationId.empty());
    return m_conversationId;
}

int CSpxTurnStatusEventArgs::GetStatusCode() const
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_interactionId.empty());
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_conversationId.empty());
    return m_status;
}

void CSpxTurnStatusEventArgs::Init(std::string interactionId, std::string conversationId, int status)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, interactionId.empty());
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, conversationId.empty());
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_interactionId.empty());
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_conversationId.empty());
    m_interactionId = std::move(interactionId);
    m_conversationId = std::move(conversationId);
    m_status = status;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
