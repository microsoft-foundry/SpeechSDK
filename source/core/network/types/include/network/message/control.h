//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>

#include "ajv.h"
#include "network/message/visiting_json_reader_view.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

namespace Message
{
    struct Control
    {
        explicit Control(std::string type):
            Type{ std::move(type) },
            JSON{}
        {}

        explicit Control(VisitingJsonReaderView reader):
            Type{ reader.GetString("type") },
            JSON{ reader.GetUnvisitedElements() }
        {}

        std::string Type;
        ajv::JsonBuilder JSON;

        ajv::JsonBuilder Serialize() const
        {
            ajv::JsonBuilder json{};
            json["type"] = Type;
            json |= JSON;
            return json;
        }
    };
}

} } } }
