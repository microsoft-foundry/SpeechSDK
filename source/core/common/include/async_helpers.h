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

inline SPXHR async_operation_wait_for(SPXASYNCHANDLE async_handle, uint32_t milliseconds)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto async_handles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<void>, SPXASYNCHANDLE>();
        auto async_operation = (*async_handles)[async_handle];
        auto completed = async_operation->WaitFor(milliseconds);
        if (!completed)
        {
            return SPXERR_TIMEOUT;
        }
        async_operation->Future.get();
        return SPX_NOERROR;
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

template<typename Result>
SPXHR async_operation_wait_for(SPXASYNCHANDLE async_handle, uint32_t milliseconds, SPXHANDLE* result_handle)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, result_handle == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *result_handle = SPXHANDLE_INVALID;

        auto async_handles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<std::shared_ptr<Result>>, SPXASYNCHANDLE>();
        auto async_operation = (*async_handles)[async_handle];
        auto completed = async_operation->WaitFor(milliseconds);
        if (!completed)
        {
            return SPXERR_TIMEOUT;
        }
        auto result = async_operation->Future.get();
        if (result == nullptr)
        {
            return SPXERR_TIMEOUT;
        }
        auto result_handles = CSpxSharedPtrHandleTableManager::Get<Result, SPXRESULTHANDLE>();
        *result_handle = result_handles->TrackHandle(result);
        return SPX_NOERROR;
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

template<typename Result>
SPXHR async_operation_wait_for_untracked(SPXASYNCHANDLE async_handle, uint32_t milliseconds, Result* resultPtr)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, resultPtr == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *resultPtr = SPXHANDLE_INVALID;

        auto async_handles = CSpxSharedPtrHandleTableManager::Get<CSpxAsyncOp<Result>, SPXASYNCHANDLE>();
        auto async_operation = (*async_handles)[async_handle];
        auto completed = async_operation->WaitFor(milliseconds);
        if (!completed)
        {
            return SPXERR_TIMEOUT;
        }
        auto result = async_operation->Future.get();
        *resultPtr = result;
        return SPX_NOERROR;
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

// Used to batch a common async operatern pattern across the C API.
//  1. Call the provided async operation function, which will populate an async handle. Errors in this operation
//      function SHOULD be propagated, as they're precondition failures in the invocation that happen "before the
//      async part starts."
//  2. Call the provided async wait function, which will block until the async handle is signalled complete. Errors in
//      this portion SHOULD NOT be propagated, as they're late-binding failures that happen "after the async part
//      starts" and should be evented, instead.
template<typename OperationFn, typename WaitFn, typename... Args>
inline SPXHR async_to_sync(SPXHANDLE handle, OperationFn operationFn, WaitFn waitFn, Args&&... args)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        SPXHANDLE async_handle = SPXHANDLE_INVALID;
        auto guard = Utils::MakeScopeGuard([&]()
            {
                SPX_REPORT_ON_FAIL(recognizer_async_handle_release(async_handle));
            });
        hr = operationFn(handle, std::forward<Args&&>(args)..., &async_handle);
        SPX_RETURN_ON_FAIL(hr);
        hr = waitFn(async_handle, UINT32_MAX);
        SPX_REPORT_ON_FAIL(hr);
        return SPX_NOERROR;
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

// Used to batch a common async operatern pattern across the C API.
//  1. Call the provided async operation function, which will populate an async handle. Errors in this operation
//      function SHOULD be propagated, as they're precondition failures in the invocation that happen "before the
//      async part starts."
//  2. Call the provided async wait function, which will block until the async handle is signalled complete. Errors in
//      this portion SHOULD NOT be propagated, as they're late-binding failures that happen "after the async part
//      starts" and should be evented, instead.
//  3. Call the provided handle close function to clean up the handle.
template<typename OperationFn, typename WaitFn, typename CloseFn, typename... Args>
SPXHR async_to_sync_with_result(SPXHANDLE handle, SPXHANDLE* resultHandle, OperationFn operationFn, WaitFn waitFn, CloseFn closeFn, Args&&... args)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        SPXHANDLE async_handle = SPXHANDLE_INVALID;
        auto guard = Utils::MakeScopeGuard([&]()
            {
                SPX_REPORT_ON_FAIL(closeFn(async_handle));
            });
        hr = operationFn(handle, std::forward<Args&&>(args)..., &async_handle);
        SPX_RETURN_ON_FAIL(hr);
        hr = waitFn(async_handle, UINT32_MAX, resultHandle);
        SPX_REPORT_ON_FAIL(hr);
        return SPX_NOERROR;
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}


template<typename I, typename F, typename... Args>
void launch_async_op(I& obj, F I::* member, SPXASYNCHANDLE *asyncHandle, Args&&... args)
{
    *asyncHandle = SPXHANDLE_INVALID;
    using async_type = decltype((std::declval<I>().*member)(std::declval<Args>()...));
    auto asyncOp = std::make_shared<async_type>((obj.*member)(std::forward<Args>(args)...));
    auto asyncTable = CSpxSharedPtrHandleTableManager::Get<async_type, SPXASYNCHANDLE>();
    *asyncHandle = asyncTable->TrackHandle(asyncOp);
}

} } } }
