//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// handle_helpers.h: Helper methods for SPX*HANDLE related capabilities
//

#pragma once
#include <speechapi_c_common.h>
#include "interfaces/ispx_init_from_properties.h"
#include "create_object_helpers.h"
#include "try_catch_helpers.h"
#include "handle_table.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "site_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxApiManager : public CSpxSharedPtrHandleTableManager
{
public:

    template <typename ResultT, typename... Args>
    using InvokeFnT = ResultT(*)(Args...);

    template<typename Handle, typename T, typename... Args>
    static SPXHR HandleTableFn(InvokeFnT<SPXHR, CSpxHandleTable<T, Handle>*, Args...> fnPtr, Args... args) noexcept
    {
        SPXAPI_INIT_HR_TRY(hr)
        {
            auto handletable = CSpxSharedPtrHandleTableManager::Get<T, Handle>();
            hr = fnPtr(handletable, std::forward<Args>(args)...);
        }
        SPXAPI_CATCH_AND_RETURN_HR(hr)
    }

    template<typename Handle, typename T, typename... Args>
    static SPXHR HandleFn(Handle handle, InvokeFnT<SPXHR, CSpxHandleTable<T, Handle>*, Handle, Args...> fnPtr, Args... args) noexcept
    {
        SPXAPI_INIT_HR_TRY(hr)
        {
            auto handletable = CSpxSharedPtrHandleTableManager::Get<T, Handle>();

            SPX_IFTRUE_EXITFN_CLEANUP(handle == nullptr, hr = SPXERR_INVALID_ARG);
            hr = fnPtr(handletable, handle, std::forward<Args>(args)...);
        }
        SPXAPI_CATCH_CLEANUP_AND_RETURN_HR(hr)
    }

    template<typename Handle, typename T, typename... Args>
    static bool HandleFnNoError(Handle handle, InvokeFnT<SPXHR, CSpxHandleTable<T, Handle>*, Handle, Args...> fnPtr, Args... args) noexcept
    {
        SPXAPI_INIT_HR_TRY(hr)
        {
            auto handletable = CSpxSharedPtrHandleTableManager::Get<T, Handle>();

            SPX_IFTRUE_EXITFN_CLEANUP(handle == nullptr, hr = SPXERR_INVALID_ARG);
            hr = fnPtr(handletable, handle, std::forward<Args>(args)...);
        }
        SPXAPI_CATCH_CLEANUP_AND_RETURN(hr, hr == SPX_NOERROR);
    }

    template<typename Handle, typename T, typename... Args>
    static SPXHR PtrFn(Handle handle, InvokeFnT<SPXHR, CSpxHandleTable<T, Handle>*, Handle, std::shared_ptr<T>, Args...> fnPtr, Args... args) noexcept
    {
        SPXAPI_INIT_HR_TRY(hr)
        {
            auto handletable = CSpxSharedPtrHandleTableManager::Get<T, Handle>();

            SPX_IFTRUE_EXITFN_CLEANUP(handle == nullptr, hr = SPXERR_INVALID_ARG);
            auto ptr = handletable->TryGetPtr(handle);

            SPX_IFTRUE_EXITFN_CLEANUP(ptr == nullptr, hr = SPXERR_INVALID_HANDLE);
            hr = fnPtr(handletable, handle, std::move(ptr), std::forward<Args>(args)...);
        }
        SPXAPI_CATCH_CLEANUP_AND_RETURN_HR(hr)
    }

    template<typename Handle, typename I1, typename I2, typename... Args>
    static SPXHR PtrQiFn(Handle handle, InvokeFnT<SPXHR, CSpxHandleTable<I1, Handle>*, Handle, std::shared_ptr<I1>, std::shared_ptr<I2>, Args...> fnPtr, Args... args) noexcept
    {
        return PtrFn<Handle, I1, decltype(fnPtr), Args...>(handle, PtrQiFnImpl<Handle, I1, I2, Args...>, fnPtr, std::forward<Args>(args)...);
    }

    template<typename Handle, typename T, typename... Args>
    static SPXHR CreateFnTrack(const char* className, InvokeFnT<std::shared_ptr<ISpxGenericSite>> fnSite, Handle* handle, InvokeFnT<SPXHR, std::shared_ptr<T>, Args...> fnPtr, Args... args) noexcept
    {
        SPXAPI_INIT_HR_TRY(hr)
        {
            SPX_IFTRUE_EXITFN_CLEANUP(handle == nullptr, hr = SPXERR_INVALID_ARG);
            *handle = SPXHANDLE_INVALID;

            auto ptr = SpxCreateObjectWithSite<ISpxPhrase>(className, fnSite());
            SPX_IFTRUE_EXITFN_CLEANUP(ptr == nullptr, hr = SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE);

            hr = fnPtr(ptr, std::forward<Args>(args)...);
            if (SPX_SUCCEEDED(hr))
            {
                *handle = TrackHandle<T, Handle>(ptr);
            }
        }
        SPXAPI_CATCH_CLEANUP_AND_RETURN_HR(hr);
    }

    template<typename Handle, typename I1, typename I2, typename... Args>
    static SPXHR CreateFnQiTrack(const char* className, InvokeFnT<std::shared_ptr<ISpxGenericSite>> fnSite, Handle* handle, InvokeFnT<SPXHR, std::shared_ptr<I1>, Args...> fnPtr, Args... args) noexcept
    {
        SPXAPI_INIT_HR_TRY(hr)
        {
            SPX_IFTRUE_EXITFN_CLEANUP(handle == nullptr, hr = SPXERR_INVALID_ARG);
            *handle = SPXHANDLE_INVALID;

            auto ptr1 = SpxCreateObjectWithSite<I1>(className, fnSite());
            SPX_IFTRUE_EXITFN_CLEANUP(ptr1 == nullptr, hr = SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE);

            hr = fnPtr(ptr1, std::forward<Args>(args)...);
            if (SPX_SUCCEEDED(hr))
            {
                *handle = TrackHandle<I2, Handle>(SpxQueryInterface<I2>(ptr1));
            }
        }
        SPXAPI_CATCH_CLEANUP_AND_RETURN_HR(hr);
    }


    // PtrFn:           GetPtr(), fn(..., ptr)
    // PtrQiFn:         GetPtr(), Qi(), fn(..., ptr, qi)
    // PtrQsFn:         GetPtr(), Qs(), fn(..., ptr, qs)

    // CreateFnTrack:     SpxCreateObjectWithSite(..., root), fn(..., ptr), trackPtr ? Track(ptr)
    // CreateFnQiTrack:   SpxCreateObjectWithSite(..., root), fn(..., ptr), Qi(), Track(qi)
    // CreateFnQsTrack:   SpxCreateObjectWithSite(..., root), fn(..., ptr), Qs(), Track(qs)
    // CreateQiFnTrack:   SpxCreateObjectWithSite(..., root), Qi(), fn(..., ptr, qi), trackPtr ? Track(ptr) : trackQi ? Track(qi)
    // CreateQsFnTrack:   SpxCreateObjectWithSite(..., root), Qs(), fn(..., ptr, qs), trackPtr ? Track(ptr) : trackQs ? Track(qs)

    template<typename Handle, typename T>
    static bool IsValid(Handle handle) noexcept
    {
        return HandleFnNoError<Handle, T>(handle, CSpxApiManager::IsValidFnImpl<Handle, T>);
    }

    template<typename Handle, typename T>
    static SPXHR Release(Handle handle) noexcept
    {
        return HandleFn<Handle, T>(handle, CSpxApiManager::ReleaseFnImpl<Handle, T>);
    }

    template<typename Handle, typename T>
    static SPXHR ReleaseAlwaysNoError(Handle handle) noexcept
    {
        return Release<Handle, T>(handle), SPX_NOERROR;
    }

    template<typename Handle, typename T>
    static SPXHR TermAndClear(Handle handle) noexcept
    {
        return PtrFn<Handle, T>(handle, TermAndClearFnImpl<Handle, T>);
    }

    template<typename Handle1, typename I1, typename Handle2, typename I2>
    static SPXHR QueryInterface(Handle1 handle1, Handle2* handle2) noexcept
    {
        SPX_IFTRUE(handle2 != nullptr, *handle2 = nullptr); // don't return early if nullptr (size optimization), but guarantee *handle2=nullptr (if possible)
        return PtrFn<Handle1, I1, Handle2*>(handle1, TrackQueryInterfaceFnImpl<Handle1, I1, Handle2, I2>, handle2);
    }

    template<typename Handle1, typename I1, typename Handle2, typename I2>
    static SPXHR QueryInterfaceAlwaysNoError(Handle1 handle1, Handle2* handle2) noexcept
    {
        return QueryInterface<Handle1, I1, Handle2, I2>(handle1, handle2), SPX_NOERROR;
    }

    template<typename Handle1, typename I1, typename Handle2, typename I2>
    static SPXHR QueryService(Handle1 handle1, Handle2* handle2) noexcept
    {
        SPX_IFTRUE(handle2 != nullptr, *handle2 = nullptr); // don't return early if nullptr (size optimization), but guarantee *handle2=nullptr (if possible)
        return PtrFn<Handle1, I1, Handle2*>(handle1, TrackQueryServiceFnImpl<Handle1, I1, Handle2, I2>, handle2);
    }

protected:

    template<typename Handle, typename T>
    static SPXHR IsValidFnImpl(CSpxHandleTable<T, Handle>* table, Handle handle)
    {
        return table->IsTracked(handle) ? SPX_NOERROR : SPXERR_INVALID_HANDLE;
    }

    template<typename Handle, typename T>
    static SPXHR ReleaseFnImpl(CSpxHandleTable<T, Handle>* table, Handle handle)
    {
        return table->StopTracking(handle) ? SPX_NOERROR : SPXERR_INVALID_HANDLE;
    }

    template<typename Handle, typename T>
    static SPXHR TermAndClearFnImpl(CSpxHandleTable<T, Handle>* table, Handle handle, std::shared_ptr<T> ptr)
    {
        SpxTermAndClearNothrow(ptr);
        return table->StopTracking(handle) ? SPX_NOERROR : SPXERR_INVALID_HANDLE;
    }

    template<typename Handle, typename I1, typename I2, typename... Args>
    static SPXHR PtrQiFnImpl(CSpxHandleTable<I1, Handle>* table, Handle handle, std::shared_ptr<I1> ptr, InvokeFnT<SPXHR, CSpxHandleTable<I1, Handle>*, Handle, std::shared_ptr<I1>, std::shared_ptr<I2>, Args...> fnPtr, Args... args)
    {
        auto ptr2 = SpxQueryInterface<I2>(ptr);
        return ptr2 != nullptr
            ? fnPtr(table, handle, ptr, ptr2, std::forward<Args>(args)...)
            : SPXERR_RUNTIME_ERROR;
    }

    template<typename Handle1, typename I1, typename Handle2, typename I2>
    static SPXHR TrackQueryInterfaceFnImpl(CSpxHandleTable<I1, Handle1>*, Handle1, std::shared_ptr<I1> ptr1, Handle2* handle2)
    {
        auto ptr2 = SpxQueryInterface<I2>(ptr1);
        auto hr = ptr2 == nullptr ? SPXERR_INVALID_HANDLE : handle2 == nullptr ? SPXERR_INVALID_ARG : SPX_NOERROR;
        if (hr == SPX_NOERROR)
        {
            *handle2 = TrackHandle<I2, Handle2>(ptr2);
        }
        return hr;
    }

    template<typename Handle1, typename I1, typename Handle2, typename I2>
    static SPXHR TrackQueryServiceFnImpl(CSpxHandleTable<I1, Handle1>*, Handle1, std::shared_ptr<I1> ptr1, Handle2* handle2)
    {
        auto ptr2 = SpxQueryService<I2>(ptr1);
        auto hr = ptr2 == nullptr ? SPXERR_INVALID_HANDLE : handle2 == nullptr ? SPXERR_INVALID_ARG : SPX_NOERROR;
        if (hr == SPX_NOERROR)
        {
            *handle2 = TrackHandle<I2, Handle2>(ptr2);
        }
        return hr;
    }
};

template<typename T, typename Handle = SPXHANDLE>
std::shared_ptr<T> SpxGetPtrFromHandle(Handle handle)
{
    auto handletable = CSpxSharedPtrHandleTableManager::Get<T, Handle>();
    return handletable->GetPtr(handle);
}

template<typename T, typename Handle = SPXHANDLE>
std::shared_ptr<T> SpxTryGetPtrFromHandle(Handle handle)
{
    auto handletable = CSpxSharedPtrHandleTableManager::Get<T, Handle>();
    return handletable->TryGetPtr(handle);
}

template<typename T, typename Handle = SPXHANDLE>
std::shared_ptr<T> SpxTryGetPtrFromHandleOrRootSite(SPXHANDLE handle)
{
    return handle == SPXHANDLE_RESERVED1 // undocumented ability to access root interfaces
        ? SpxQueryService<T>(SpxGetRootSite())
        : SpxTryGetPtrFromHandle<T, Handle>(handle);
}


template<typename T, typename Handle = SPXHANDLE, typename T2 = ISpxNamedProperties>
SPXHR SpxHandleCreateNoThrow(Handle* handle, const char* className, const char* optionName, const char* optionValue, SPXHANDLE moreOptions, const char* moreNamespace = nullptr) noexcept
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_IFTRUE_EXITFN_CLEANUP(handle == nullptr, hr = SPXERR_INVALID_ARG);
        *handle = SPXHANDLE_INVALID;

        SPX_IFTRUE_EXITFN_CLEANUP((optionName == nullptr) != (optionValue == nullptr), hr = SPXERR_INVALID_ARG);
        SPX_IFTRUE_EXITFN_CLEANUP(moreOptions == SPXHANDLE_INVALID, hr = SPXERR_INVALID_HANDLE);

        auto root = SpxGetRootSite();
        auto ptr = SpxCreateObjectWithSite<T>(className, root);
        SPX_IFTRUE_EXITFN_CLEANUP(ptr == nullptr, hr = SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE);

        auto more = SpxTryGetPtrFromHandle<T2>(moreOptions);
        auto properties = more != nullptr
            ? SpxQueryInterface<ISpxNamedProperties>(more)
            : SpxTryGetPtrFromHandle<ISpxNamedProperties>(moreOptions);
        SPX_IFTRUE_EXITFN_CLEANUP(properties == nullptr && moreOptions != nullptr, hr = SPXERR_INVALID_HANDLE);

        auto init = SpxQueryInterface<ISpxInitFromProperties>(ptr);
        init->InitFromProperties(optionName, optionValue, properties, moreNamespace);

        *handle = CSpxSharedPtrHandleTableManager::TrackHandle<T, SPXHANDLE>(ptr);
    }
    SPXAPI_CATCH_CLEANUP_AND_RETURN_HR(hr);
}

template<typename T, typename U>
std::shared_ptr<U> SpxHandleQueryInterface(SPXHANDLE handle)
{
    auto obj = SpxGetPtrFromHandle<T>(handle);
    return SpxQueryInterface<U>(obj);
}

template<typename T, typename U>
std::shared_ptr<U> SpxTryHandleQueryInterface(SPXHANDLE handle)
{
    auto obj = SpxTryGetPtrFromHandle<T>(handle);
    if (obj)
    {
        return SpxQueryInterface<U>(obj);
    }
    return nullptr;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
