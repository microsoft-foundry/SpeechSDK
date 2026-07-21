//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <inttypes.h>
#include <sstream>
#include <stdio.h>
#include <iomanip>
#include <cmath>
#include "time_utils.h"

using namespace std::chrono;

namespace PAL
{
    constexpr uint8_t MAX_MILLISECOND_DIGITS = 7;

    std::string GetUtcTimestamp(uint8_t milliSecondDigits)
    {
        return GetTimeInString(std::chrono::system_clock::now(), milliSecondDigits);
    }

    std::string GetTimeInString(const system_clock::time_point& t, uint8_t milliSecondDigits)
    {
        if (milliSecondDigits > MAX_MILLISECOND_DIGITS)
        {
            milliSecondDigits = MAX_MILLISECOND_DIGITS;
        }

        constexpr size_t length = 128;
        char buffer[length];
        time_t rawtime;
        struct tm timeinfo;

        rawtime = system_clock::to_time_t(t);
#ifdef _MSC_VER
        gmtime_s(&timeinfo, &rawtime);
#else
        gmtime_r(&rawtime, &timeinfo);
#endif

        size_t bytesWritten = strftime(buffer, length, "%FT%T", &timeinfo);
        if (bytesWritten == 0)
        {
            return "";
        }

        if (milliSecondDigits > 0)
        {
            uint64_t subSecondPeriods = t.time_since_epoch().count() * system_clock::period::num % system_clock::period::den;
            uint64_t ticks = subSecondPeriods * 10000000 * system_clock::period::num / system_clock::period::den;

            // TODO: should round here instead of truncating?
            uint64_t truncatedTicks = ticks / static_cast<uint64_t>(std::pow(10, (MAX_MILLISECOND_DIGITS - milliSecondDigits)));

            // Use a literal format string with width specifier
            int ret = snprintf(buffer + bytesWritten, length - bytesWritten, ".%0*" PRIu64 "Z", milliSecondDigits, truncatedTicks);
            if (ret < 0)
            {
                return "";
            }
            bytesWritten += ret;
        }
        else
        {
            bytesWritten += snprintf(buffer + bytesWritten, length - bytesWritten, "Z");
        }

        return buffer;
    }

    uint64_t GetTicks(const system_clock::duration& t)
    {
        constexpr uint64_t nanosecondsInTick = 100;
        nanoseconds durationInNanoseconds = t;
        return durationInNanoseconds.count() / nanosecondsInTick;
    }

    bool TryParseUtcTimestamp(const std::string& utcTimestamp, std::chrono::system_clock::time_point& localTime)
    {
        // std::get_time can't handle millisecond conversion. Since we expect a timestamp string to
        // be in the following format:
        // 2023-12-23T18:23:23.1234567Z
        // We can split off the millisecond part, do the conversion of up to the second part first.
        // Then we can convert the milliseconds to ticks, and add those to the time

        size_t milliSecondIndex = utcTimestamp.find('.');
        if (milliSecondIndex == std::string::npos)
        {
            return false;
        }

        size_t zuluIndex = utcTimestamp.rfind('Z');
        if (zuluIndex == std::string::npos)
        {
            return false;
        }

        // max allowed is 7 digits for the milliseconds portion
        size_t numMilliDigits = zuluIndex - milliSecondIndex - 1;
        if (numMilliDigits > 7)
        {
            return false;
        }

        bool success = TryParseUtcTimeString(
            utcTimestamp.substr(0, milliSecondIndex),
            "%Y-%m-%dT%H:%M:%S",
            localTime);
        if (!success)
        {
            return false;
        }

        try
        {
            char* remaining = nullptr;
            size_t ticks = std::strtoul(utcTimestamp.c_str() + milliSecondIndex + 1, &remaining, 10);
            if (remaining == nullptr || *remaining != 'Z')
            {
                return false;
            }

            // convert to nanoseconds (1 tick = 100ns)
            size_t ns = ticks * static_cast<size_t>(std::pow(10, 9 - numMilliDigits));
            localTime += std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds{ ns });

            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    bool TryParseUtcTimeString(const std::string& timeString, const char* format, system_clock::time_point& localTime)
    {
        // Attempt to parse the date time string
        std::tm utc_tm;

        std::istringstream ss(timeString);
        ss.imbue(std::locale("")); // force to C locale for consistent parsing
        ss >> std::get_time(&utc_tm, format);

        if (ss.fail())
        {
            return false;
        }

        std::time_t t;

#ifdef _MSC_VER
        t = _mkgmtime(&utc_tm);
#else
        t = timegm(&utc_tm);
#endif

        if (t == static_cast<time_t>(-1))
        {
            // failed to convert to a time_t
            return false;
        }

        localTime = system_clock::from_time_t(t);
        return true;
    }

    uint64_t GetMillisecondsSinceEpoch()
    {
        const auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return ms.count();
    }
}


