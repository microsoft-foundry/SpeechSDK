//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>
#include <type_traits>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    #define ENUM_PARSE(val, str)            \
        if (PAL::stricmp(str, string) == 0) \
        {                                   \
            value = val;                    \
            return true;                    \
        }

    #define ENUM_N_PARSE(val, str)                        \
        if (PAL::strnicmp(str, string, strlen(str)) == 0) \
        {                                                 \
            value = val;                                  \
            return true;                                  \
        }

    // Ideally we'd use std::false_type::value here instead but this causes build breaks in GCC
    template<typename... T>
    constexpr bool always_false = false;

    /// <summary>
    /// Helpers for working with enumerations
    /// </summary>
    class EnumHelpers
    {
    public:
        /// <summary>
        /// Converts an enum value to its string representation
        /// </summary>
        /// <typeparam name="TEnum">The type of the enumeration</typeparam>
        /// <param name="value">The value to convert</param>
        /// <returns>The corresponding string for that value</returns>
        template<typename TEnum>
        static const char* ToString(TEnum value)
        {
            (void)value;    // make Clang happy
            static_assert(always_false<TEnum>, "You must specialize the ToString for that enum type to use this method");
            return nullptr; // will never get here but GCC gets very upset without this
        }

        /// <summary>
        /// Converts a string representation of enum back into the enum
        /// </summary>
        /// <typeparam name="TEnum">The type of the enumeration</typeparam>
        /// <param name="string">The string to convert</param>
        /// <param name="value">The value to set</param>
        /// <returns>True if the string is a valid enum value, false otherwise</returns>
        template<typename TEnum>
        static bool TryParse(const char* string, TEnum& value)
        {
            (void)string;   // make clang happy
            (void)value;    // make clang happy
            static_assert(always_false<TEnum>, "You must specialize TryParse for that enum type to use this method");
            return false; // will never get here but GCC gets very upset without this
        }
    };

}}}}
