//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "connection_message_event_args.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

CSpxConnectionMessageEventArgs::CSpxConnectionMessageEventArgs()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxConnectionMessageEventArgs::~CSpxConnectionMessageEventArgs()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

std::shared_ptr<ISpxConnectionMessage> CSpxConnectionMessageEventArgs::GetMessage() const
{
    return m_message;
}

void CSpxConnectionMessageEventArgs::Init(std::shared_ptr<ISpxConnectionMessage> message)
{
    m_message = message;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
