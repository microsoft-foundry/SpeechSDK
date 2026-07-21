//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#pragma once

#include <string>

#include "util/maybe.h"

namespace PAL {

template<typename T>
using Maybe = Microsoft::CognitiveServices::Speech::Impl::Maybe<T>;


std::string demangle(const char* name);

struct OperatingSystemInfo {
    std::string platform;
    std::string name;
    std::string version;

    std::string to_string()
    {
        return platform + " " + name + " " + version;
    }
};

OperatingSystemInfo getOperatingSystem();

Maybe<std::string> SpxGetEnv(const char* name);

} // PAL
