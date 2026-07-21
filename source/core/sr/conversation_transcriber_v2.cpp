//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// conversation_transcriber.cpp: implementation declarations for conversation transcriber
//

#include "stdafx.h"
#include <sstream>
#include "create_object_helpers.h"
#include "conversation_transcriber_v2.h"
#include "spx_namespace.h"
#include "usp.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxConversationTranscriberV2::CSpxConversationTranscriberV2()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxConversationTranscriberV2::~CSpxConversationTranscriberV2()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}
}}}} // Microsoft::CognitiveServices::Speech::Impl
