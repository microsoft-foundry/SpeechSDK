//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "interfaces/http_method.h"
#include "interfaces/enum_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Converts an enum value to its string representation
    /// </summary>
    /// <param name="value">The value to convert</param>
    /// <returns>The corresponding string for that value</returns>
    template<>
    const char* EnumHelpers::ToString<HttpMethod>(HttpMethod value);

    /// <summary>
    /// Converts a string representation of enum back into the enum
    /// </summary>
    /// <param name="string">The string to convert</param>
    /// <param name="value">The value to set</param>
    /// <returns>True if the string is a valid enum value, false otherwise</returns>
    template<>
    bool EnumHelpers::TryParse<HttpMethod>(const char* string, HttpMethod& value);

}}}}
