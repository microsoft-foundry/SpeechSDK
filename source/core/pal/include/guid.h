//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>

namespace PAL {

    constexpr size_t UUID_LENGTH = 36;

    std::string GenerateGUID();

}
