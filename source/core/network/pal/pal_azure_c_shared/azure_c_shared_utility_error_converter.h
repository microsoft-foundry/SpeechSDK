//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// Error converter for azure_c_shared_utility library error codes
// Provides detailed, actionable error messages for network connection failures
//

#pragma once

#include <string>
#include <cstdint>

#ifndef _MSC_VER
#pragma GCC system_header
#endif
#include <azure_c_shared_utility/httpapi.h>
#include <azure_c_shared_utility/macro_utils.h>
#include <azure_c_shared_utility/uws_client.h>

std::string GetAzureCSharedErrorMessage(int32_t errorCode);

// Get a detailed, actionable error message for WebSocket connection failures.
std::string GetConnectionErrorMessage(WS_OPEN_RESULT wsResult, int underlyingCode);
