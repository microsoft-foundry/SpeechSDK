//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#pragma once
#include <string>
#include <chrono>

namespace PAL
{
    /// <summary>
    /// Gets the current UTC time as a formatted string. The string will have a format similar to
    /// 2022-12-23T14:22:33.1234567
    /// </summary>
    /// <param name="milliSecondDigits">How many digits of precision you want to display for milliseconds. Min 0, max 7</param>
    /// <returns>A UTC timestamp string</returns>
    std::string GetUtcTimestamp(uint8_t milliSecondDigits = 7);

    /// <summary>
    /// Returns the string that represents the corresponding UTC time for the specified time point. The string
    /// will have a format similar to 2022-12-23T14:22:33.123
    /// </summary>
    /// <param name="t">The time point to convert to a UTC time string</param>
    /// <param name="milliSecondDigits">How many digits of precision you want to display for milliseconds. Min 0, max 7</param>
    /// <returns>The corresponding UTC time string</returns>
    std::string GetTimeInString(const std::chrono::system_clock::time_point& t, uint8_t milliSecondDigits = 7);

    uint64_t GetTicks(const std::chrono::system_clock::duration& t);

    /// <summary>
    /// Parses a UTC timestamp string (e.g. 2023-03-02T20:48:16.1234567Z) into a local time
    /// </summary>
    /// <param name="utcTimestamp">The UTC timestamp string to parse</param>
    /// <param name="localTime">The parsed local time to set</param>
    /// <returns>True if the string was parsed successfull, false otherwise</returns>
    bool TryParseUtcTimestamp(const std::string& utcTimestamp, std::chrono::system_clock::time_point& localTime);

    /// <summary>
    /// Parses a UTC time from a string using the specified format. The time set will be local.
    /// </summary>
    /// <param name="timeString">The time string to parse</param>
    /// <param name="format">The format string. Please refer to the C++ 'get_time' documentation for
    /// valid values</param>
    /// <param name="time">The parsed local time to set</param>
    /// <returns>True if the string was parsed successfully, false otherwise</returns>
    bool TryParseUtcTimeString(const std::string& timeString, const char* format, std::chrono::system_clock::time_point& localTime);

    uint64_t GetMillisecondsSinceEpoch();
}
