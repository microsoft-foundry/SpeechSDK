//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_module_object.h: Implementation declarations for *CreateModuleObject* methods
//

#pragma once

#include <future>
#include <memory>
#include <map>
#include <string>

#ifdef _DEBUG
#define SPX_CONFIG_DBG_TRACE_ALL 1
#define SPX_CONFIG_TRACE_ALL 1
#else
#define SPX_CONFIG_TRACE_ALL 1
#endif

#include "spxcore_common.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


SPX_EXTERN_C void* TTS_Cloud_CreateModuleObject(const char* className, uint64_t interfaceTypeId);


} } } } // Microsoft::CognitiveServices::Speech::Impl
