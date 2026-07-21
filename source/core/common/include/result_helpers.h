//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "spxcore_common.h"
#include "handle_helpers.h"
#include "speechapi_cxx_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<typename T, typename GetFn>
static const char* result_get_char_property(SPXRESULTHANDLE hresult, GetFn getFn)
{
    char* value = nullptr;

    if (hresult == nullptr)
    {
        return value;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto result = SpxGetPtrFromHandle<T>(hresult);
        auto fn = std::bind(getFn, result.get());
        const auto tempValue = fn();
        const auto size = tempValue.size() + 1;
        value = new char[size];
        PAL::strcpy(value, size, tempValue.c_str(), size, true);
    }
    SPXAPI_CATCH_AND_RETURN(hr, value);
}

} } } }