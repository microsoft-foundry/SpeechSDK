//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

// C++ Standard Library headers

#ifdef _DEBUG
#define SPX_CONFIG_DBG_TRACE_ALL 1
#define SPX_CONFIG_TRACE_ALL 1
#else
#define SPX_CONFIG_TRACE_ALL 1
#endif

// No need to define __declspec(dllexport) in core modules other than c_api.
#ifdef _WIN32
#ifndef SPXAPI_EXPORT
#define SPXAPI_EXPORT
#endif
#endif

// Project core headers
#include "interface_helpers.h"

// PAL interface headers
#include "i_http_platform.h"
#include "interfaces/ispx_http_request.h"
#include "interfaces/ispx_http_response.h"
#include "interfaces/web_socket.h"

// Common error codes and macros
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif
