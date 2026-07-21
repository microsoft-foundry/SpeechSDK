//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <cstring>

#include "test_utils.h"
#include "platform.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXTEST_CASE_BEGIN("PAL::getOperatingSystem - glibc version appended", "[platform][linux]")
{
    auto osInfo = PAL::getOperatingSystem();

    constexpr const char* glibcPrefix = "glibc/";

#if defined(__linux__) && defined(__has_include) && __has_include(<gnu/libc-version.h>)
    auto pos = osInfo.name.find(glibcPrefix);
    SPXTEST_REQUIRE(pos != std::string::npos);

    auto glibcVersion = osInfo.name.substr(pos + std::strlen(glibcPrefix));
    SPXTEST_REQUIRE(!glibcVersion.empty());
#else
    // Non-Linux or non-glibc platforms should not report glibc.
    SPXTEST_REQUIRE(osInfo.name.find(glibcPrefix) == std::string::npos);
#endif
}
SPXTEST_CASE_END()
