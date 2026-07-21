//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// service_provider_impl.h: Implementation declarations for ISpxServiceProviderImpl C++ class
//

#pragma once

#include "interfaces/service_provider.h"
#include "interface_helpers.h"
#include "spxcore_common.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class ISpxAddServiceProviderImpl : public ISpxAddServiceProvider
{
public:

    // --- ISpxAddServiceProvider
    void AddService(uint64_t serviceTypeId, std::shared_ptr<ISpxInterfaceBase> service) override
    {
        return InternalAddService(serviceTypeId, service);
    }

protected:

    template <class I>
    std::shared_ptr<I> InternalQueryService() const
    {
        auto service = InternalQueryService(Type<I>::Id);
        return SpxQueryInterface<I>(service);
    }

    std::shared_ptr<ISpxInterfaceBase> InternalQueryService(uint64_t serviceTypeId) const
    {
        auto item = m_services.find(serviceTypeId);
        if (item != m_services.end())
        {
            return item->second;
        }

        return nullptr;
    }

    template <class T>
    void InternalAddService(std::shared_ptr<T> service)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, service == nullptr);
        InternalAddService(Type<T>::Id, SpxQueryInterface<ISpxInterfaceBase>(service));
    }

    void InternalAddService(uint64_t serviceTypeId, std::shared_ptr<ISpxInterfaceBase> service)
    {
        auto found = m_services.find(serviceTypeId);
        if (found != m_services.end())
        {
            m_services.erase(found);
        }
        m_services.emplace(serviceTypeId, service);
    }


private:

    std::map<uint64_t, std::shared_ptr<ISpxInterfaceBase>> m_services;
};

class ISpxServiceProviderImpl : public ISpxServiceProvider, public ISpxAddServiceProviderImpl
{
    public:

    // --- ISpxServiceProvider
    std::shared_ptr<ISpxInterfaceBase> QueryService(uint64_t serviceTypeId) override
    {
        return InternalQueryService(serviceTypeId);
    }
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
