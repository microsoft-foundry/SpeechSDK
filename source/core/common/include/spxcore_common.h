//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>
#include <stdlib.h>
#include <stdint.h>

#ifndef SUPPRESS_CORE_COMMON_TRACE_IMPL
#if !defined(__AZAC_DO_TRACE_IMPL) && !defined(__SPX_DO_TRACE_IMPL)
#define __SPX_DO_TRACE_IMPL diagnostics_log_trace_message
#define __AZAC_DO_TRACE_IMPL diagnostics_log_trace_message
#else
#error Neither __AZAC_DO_TRACE_IMPL nor __SPX_DO_TRACE_IMPL should be defined in compilation units including spxcore_common.h
#endif
#endif

#ifndef SUPPRESS_CORE_COMMON_THROW_IMPL
#if !defined(__AZAC_THROW_HR_IMPL) && !defined(__SPX_THROW_HR_IMPL)
#define __SPX_THROW_HR_IMPL Microsoft::CognitiveServices::Speech::Impl::ThrowWithCallstack
#else
#error Neither __AZAC_THROW_HR_IMPL nor __SPX_THROW_HR_IMPL should be defined in compilation units including spxcore_common.h
#endif
#endif

#include <speechapi_cxx_common.h>

#include "audio_constants.h"
#include "exception.h"
#include "ispxinterfaces.h"
#include <interfaces/types.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    static constexpr int MAX_JSON_PAYLOAD_FROM_USER = 52428800;

    static constexpr auto SUBSCRIPTION_KEY_NAME = "Ocp-Apim-Subscription-Key";
    static constexpr auto AUTHORIZATION_TOKEN_KEY_NAME = "Authorization";

} } } } // Microsoft::CognitiveServices::Speech::Impl
