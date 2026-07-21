//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>

#include "interfaces/object_with_site.h"
#include "interfaces/object_init.h"
#include "interface_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class T>
class ISpxObjectWithSiteInitImpl : public ISpxObjectWithSite, public ISpxObjectInit
{

public:
    // --- ISpxObjectWithSite
    void SetSite(std::weak_ptr<ISpxGenericSite> site) override
    {
        auto shared = site.lock();
        auto ptr = SpxQueryInterface<T>(shared);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, (bool)ptr != (bool)shared);

        if (m_hasSite)
        {
            Term();
            m_site.reset();
            m_hasSite = false;
        }

        m_site = ptr;
        m_hasSite = ptr.get() != nullptr;

        if (m_hasSite)
        {
            Init();
        }
    }

    // --- ISpxObjectInit
    void Init() override
    {
    }

    void Term() override
    {
    }

    // --- other public methods
    std::shared_ptr<T> GetSite() const
    {
        return m_site.lock();
    }

protected:
    ISpxObjectWithSiteInitImpl() : m_hasSite(false) {}

    template<class F>
    void InvokeOnSite(F f) const
    {
        auto site = GetSite();
        if (site != nullptr)
        {
            f(site);
        }
    }

private:
    bool m_hasSite;
    mutable std::weak_ptr<T> m_site;
};


} } } }
