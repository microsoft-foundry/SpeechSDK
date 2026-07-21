//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_operations.cpp: Implementation of common operation methods for the C API layer.
//

#include "stdafx.h"
#include "common.h"
#include <limits>
#include <tuple>
#include "event_helpers.h"
#include "handle_helpers.h"
#include "string_utils.h"
#include "service_helpers.h"
#include "async_helpers.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI speechapi_async_handle_release(SPXASYNCHANDLE h_async)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXASYNCHANDLE, CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>>>(h_async);
}

SPXAPI speechapi_async_wait_for(SPXASYNCHANDLE h_async, uint32_t milliseconds)
{
    return async_operation_wait_for(h_async, milliseconds);
}
