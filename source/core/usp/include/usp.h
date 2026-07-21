//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "usp_client_configuration.h"
#include "usp_endpoint.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

template<typename T>
using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

}}}}
