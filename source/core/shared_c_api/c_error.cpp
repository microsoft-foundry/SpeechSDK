//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "string_utils.h"
#include "handle_helpers.h"
#include "dynamic_module.h"
#include "speechapi_c_error.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

static ExceptionWithCallStack* GetException(AZAC_HANDLE errorHandle)
{
    auto errorHandles = CSpxSharedPtrHandleTableManager::Get<ExceptionWithCallStack, AZAC_HANDLE>();
    if (errorHandles->IsTracked(errorHandle))
    {
        auto error = (*errorHandles)[errorHandle];
        return error.get();
    }
    return nullptr;
}

AZAC_API_(const_char_ptr) error_get_call_stack(AZAC_HANDLE errorHandle)
{
    auto ex = GetException(errorHandle);
    return ex
        ? ex->GetCallStack()
        : nullptr;
}

AZAC_API error_get_error_code(AZAC_HANDLE errorHandle) 
{
    auto ex = GetException(errorHandle);
    return ex
        ? ex->GetErrorCode()
        : AZAC_ERR_NONE;
}

AZAC_API_(const_char_ptr) error_get_message(AZAC_HANDLE errorHandle)
{
    auto ex = GetException(errorHandle);
    return ex
        ? ex->what()
        : nullptr;
}

AZAC_API error_release(AZAC_HANDLE errorHandle)
{
    AZAC_RETURN_HR_IF(AZAC_ERR_INVALID_ARG, errorHandle == nullptr);

    AZAC_API_RESULTTYPE result = CSpxApiManager::Release<AZAC_HANDLE, ExceptionWithCallStack>(errorHandle);
    
    return result == AZAC_ERR_INVALID_HANDLE
        ? AZAC_ERR_NONE // in the old code, we returned AZAC_ERR_INVALID_HANDLE *only* if error handle was nullptr
        : result;
}
