//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

bool IsCodecAdapterAvailable();

inline bool UsingCodecByDefault()
{
// Customers usually run SDK on Linux for a service scenario, so we won't enable compressed audio transmission by default.
#if defined(_MSC_VER) || (defined(__linux__) && !defined(__ANDROID__))
    return false;
#else
    return true;
#endif
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
