//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_module_object.cpp: Implementation definitions for *CreateModuleObject* methods
//

#include "stdafx.h"

#include "factory_helpers.h"
#include "module_factory.h"
#include "site_helpers.h"
#include "site_with_thread_service.h"
#include "pal_create_module_object.h"
#include "named_properties.h"
#include "thread_service.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_EXTERN_C void* DataLib_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
SPX_EXTERN_C void* USP_CreateModuleObject(const char* className, uint64_t interfaceTypeId);

// this is the root factory for tests. We add our static libraries here so we are able to supply them
// in a mockSite in our tests.
SPX_EXTERN_C void* IntraAssemblyCreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY(CSpxSiteWithThreadService, ISpxThreadService);
        SPX_FACTORY_MAP_ENTRY(CSpxThreadService, ISpxThreadService);
        SPX_FACTORY_MAP_ENTRY(CSpxNamedProperties, ISpxNamedProperties);
        SPX_FACTORY_MAP_ENTRY_FUNC(DataLib_CreateModuleObject)
        SPX_FACTORY_MAP_ENTRY_FUNC(Pal_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY_FUNC(USP_CreateModuleObject);
    SPX_FACTORY_MAP_END();
}

void UpdateFactories(std::list<std::shared_ptr<ISpxObjectFactory>>& moduleFactories, std::shared_ptr<ISpxObjectFactory> factory)
{
    // If the factory is null, do not add it.
    if (factory == nullptr) return;

    // If the factory is already in the list, do not add it again.
    for (auto& f : moduleFactories)
    {
        if (f == factory)
        {
            return;
        }
    }
    moduleFactories.push_back(factory);
}

SPX_EXTERN_C void AddExtensionModules(std::list<std::shared_ptr<ISpxObjectFactory>>& moduleFactories)
{
    UNUSED(moduleFactories);
}

SPX_EXTERN_C SPXDLL_EXPORT void* GetModuleObject(const char* className, uint64_t interfaceTypeId)
{
    if (PAL::stricmp("CSpxResourceManager", className) == 0 && Type<ISpxGenericSite>::Id == interfaceTypeId)
    {
        auto root = SpxGetCoreRootSite();
        auto site = SpxQueryInterface<ISpxGenericSite>(root);
        return static_cast<ISpxGenericSite*>(site.get());
    }

    return nullptr;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
