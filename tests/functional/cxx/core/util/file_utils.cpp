//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "test_utils.h"

#include "file_utils.h"
#include "string_utils.h"


using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXTEST_CASE_BEGIN("PAL::ToString", "[util][files]")
{
    SPXTEST_SECTION("Latin character file names")
    {
        constexpr auto input = L"whatstheweatherlike.wav";
        std::string expected = "whatstheweatherlike.wav";
        auto output = PAL::ToString(input);
        SPXTEST_REQUIRE(output == expected);
    }

    SPXTEST_SECTION("Hindi character file names")
    {
        constexpr auto input = L"hi-IN_0_नमस्कार.wav";
        std::string expected = "hi-IN_0_नमस्कार.wav";
        auto output = PAL::ToString(input);
        SPXTEST_REQUIRE(output == expected);
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("PAL::OpenStream", "[util][files]")
{
    SPXTEST_SECTION("Latin character file names")
    {
        auto path = ROOT_RELATIVE_PATH(EMPTY_WAV_FILE);
        auto file = std::make_unique<std::fstream>();
        PAL::OpenStream(*file.get(), path, true);
        SPXTEST_REQUIRE(file->good());
    }

    SPXTEST_SECTION("Hindi character file names")
    {
        auto path = ROOT_RELATIVE_PATH(NON_LATIN_FILE_NAME);
        auto file = std::make_unique<std::fstream>();
        PAL::OpenStream(*file.get(), path, true);
        SPXTEST_REQUIRE(file->good());
    }
}
SPXTEST_CASE_END()

