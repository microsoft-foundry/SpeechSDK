//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "http_method_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    template<>
    const char* EnumHelpers::ToString(HttpMethod value)
    {
        switch (value)
        {
        case HttpMethod::Delete: return "DELETE";
        case HttpMethod::Get: return "GET";
        case HttpMethod::Patch: return "PATCH";
        case HttpMethod::Post: return "POST";
        case HttpMethod::Put: return "PUT";
        }

        return nullptr;
    }

    template<>
    bool EnumHelpers::TryParse(const char* string, HttpMethod& value)
    {
        ENUM_PARSE(HttpMethod::Delete, "DELETE");
        ENUM_PARSE(HttpMethod::Get, "GET");
        ENUM_PARSE(HttpMethod::Patch, "PATCH");
        ENUM_PARSE(HttpMethod::Post, "POST");
        ENUM_PARSE(HttpMethod::Put, "PUT");
        return false;
    }

}}}}
