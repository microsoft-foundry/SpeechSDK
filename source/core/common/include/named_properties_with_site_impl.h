//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"
#include "object_with_site_init_impl.h"
#include "service_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class ObjectWithSiteT>
class ISpxNamedPropertiesWithSiteImpl : public ISpxPropertyBagImpl
{
protected:

    // --- ISpxPropertyBagImpl (overrides)
    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const
    {
        auto ptr = const_cast<ISpxNamedPropertiesWithSiteImpl<ObjectWithSiteT>*>(this);
        auto site = static_cast<ObjectWithSiteT*>(ptr)->GetSite();
        return SpxQueryService<ISpxNamedProperties>(site);
    }
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
