//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <exception>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#include <iostream>
#include <string>

#ifdef _DEBUG
#define SPX_CONFIG_DBG_TRACE_ALL 1
#define SPX_CONFIG_TRACE_ALL 1
#else
#define SPX_CONFIG_TRACE_ALL 1
#endif

// Disable LNK4049 (caused by importing diagnostics_log_message)
#ifdef _WIN32
#define SPXAPI_EXPORT
#endif

#include "spxcore_common.h"

