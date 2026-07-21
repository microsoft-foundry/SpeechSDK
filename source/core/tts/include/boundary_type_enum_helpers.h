//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "interfaces/enum_helpers.h"
#include "speechapi_cxx_enums.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Converts a string representation of enum back into the enum
    /// </summary>
    /// <param name="string">The string to convert</param>
    /// <param name="value">The value to set</param>
    /// <returns>True if the string is a valid enum value, false otherwise</returns>
    template<>
    bool EnumHelpers::TryParse<SpeechSynthesisBoundaryType>(const char* string, SpeechSynthesisBoundaryType& value);
}}}}
