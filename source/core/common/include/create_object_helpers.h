//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_object_helpers.h: Implementation declarations/definitions for SpxCreateObject* helper methods
//

#pragma once
#include "interface_helpers.h"
#include "service_helpers.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<SPXHR hr = SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE>
inline void SpxCreateObjectError()
{
    SPX_TRACE_ERROR("site does not support ISpxObjectFactory");
    SPX_THROW_HR(hr);
}

template <class I>
inline std::shared_ptr<I> SpxCreateObject(const char* className, const std::shared_ptr<ISpxGenericSite>& site)
{
    // get the factory
    auto factory = SpxQueryService<ISpxObjectFactory>(site);
    SPX_IFTRUE(factory == nullptr, SpxCreateObjectError());

    // create the object via the factory
    return factory->CreateObject<I>(className);
}

template <class I>
inline std::shared_ptr<I> SpxCreateObjectWithSite(const char* className, ISpxGenericSite* site)
{
    auto shared = site->shared_from_this();
    return SpxCreateObjectWithSite<I>(className, shared);
}

template <class I>
inline std::shared_ptr<I> SpxCreateObjectWithSite(const char* className, ISpxInterfaceBase* site)
{
    auto _generic_ = site->QueryInterface<ISpxGenericSite>();
    return SpxCreateObjectWithSite<I>(className, _generic_);
}

template <class I>
inline std::shared_ptr<I> SpxSetSite(std::shared_ptr<I>&& ptr, const std::shared_ptr<ISpxGenericSite>& site)
{
    auto objectWithSite = SpxQueryInterface<ISpxObjectWithSite>(ptr);
    SPX_IFTRUE(objectWithSite != nullptr, objectWithSite->SetSite(site));
    return std::move(ptr);
}

template <class I>
inline std::shared_ptr<I> SpxCreateObjectWithSite(const char* className, const std::shared_ptr<ISpxGenericSite>& site)
{
    return SpxSetSite<I>(SpxCreateObject<I>(className, site), site);
}

template<typename I, SPXHR hr = SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE, typename U>
inline auto SpxCreateObjectWithSiteThrowOnFail(const char* className, U site)
{
    auto obj = SpxCreateObjectWithSite<I>(className, site);
    SPX_THROW_HR_IF(hr, !obj);
    return obj;
}

template <class T>
void SpxTerm(const std::shared_ptr<T>& ptr)
{
    if (ptr == nullptr)
        return;

    SPX_DBG_TRACE_VERBOSE("%s: ptr=0x%8p", __FUNCTION__, (void*)ptr.get());
    auto objectWithSite = SpxQueryInterface<ISpxObjectWithSite>(ptr);
    auto objectInit = SpxQueryInterface<ISpxObjectInit>(ptr);
    if (objectWithSite != nullptr)
    {
        objectWithSite->SetSite(std::weak_ptr<ISpxGenericSite>());
    }
    else if (objectInit != nullptr)
    {
        objectInit->Term();
    }
}


template <class T>
void SpxTermAndClear(std::shared_ptr<T>& ptr)
{
    if (ptr != nullptr)
    {
        SpxTerm(ptr);
        ptr = nullptr;
    }
}

template <class T>
void SpxTermAndClearNothrow(std::shared_ptr<T>& ptr) noexcept
{
    if (ptr != nullptr)
    {
        try
        {
            SpxTerm(ptr);
        }
        catch (...)
        {
            // ignored
        }
        ptr = nullptr;
    }
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
