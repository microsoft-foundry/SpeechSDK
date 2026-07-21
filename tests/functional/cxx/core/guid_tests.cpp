//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "test_utils.h"
#include "guid.h"
#include <array>
#include <algorithm>
#include <unordered_set>

static bool IsValidGuid(const std::string& guid)
{
    if (guid.size() != PAL::UUID_LENGTH)
    {
        return false;
    }

    std::array<const uint8_t, 5> sectionLengths = { 8, 4, 4, 4, 12 };

    size_t i = 0;
    for (size_t section = 0; i < PAL::UUID_LENGTH && section < sectionLengths.size(); section++)
    {
        for (size_t stopAt = i + sectionLengths[section]; i < stopAt && i < PAL::UUID_LENGTH; i++)
        {
            char c = guid[i];
            if ((c < '0' || c > '9') && (c < 'a' || c > 'f'))
            {
                return false;
            }
        }

        if (i < PAL::UUID_LENGTH && guid[i++] != '-')
        {
            return false;
        }
    }

    return true;
}

SPXTEST_CASE_BEGIN("GUID create", "[cxx][core][guid][create]")
{
    std::string guid = PAL::GenerateGUID();

    SPXTEST_CAPTURE(guid);

    SPXTEST_REQUIRE(!guid.empty());
    SPXTEST_REQUIRE(PAL::UUID_LENGTH == guid.length());
    SPXTEST_REQUIRE(IsValidGuid(guid));

} SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("GUID no collisions", "[cxx][core][guid][no_collisions]")
{
    size_t numUuids = 100000;

    std::unordered_set<std::string> allUuids(numUuids);
    bool valid = true;

    for (size_t i = 0; i < numUuids; i++)
    {
        auto pair = allUuids.insert(PAL::GenerateGUID());
        if (!IsValidGuid(*pair.first))
        {
            valid = false;
            SPXTEST_FAIL("Invalid GUID detected: " + (*pair.first));
        }
        else if (!pair.second)
        {
            valid = false;
            SPXTEST_FAIL("Detected duplicate UUID after " + std::to_string(i + 1) + " UUIDs: " + (*pair.first));
        }
    }

    SPXTEST_REQUIRE(valid);

} SPXTEST_CASE_END()
