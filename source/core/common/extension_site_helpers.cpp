//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// extension_site_helpers.cpp: Implementation definitions for SpxGetExtensionRootSite helper methods
//

#include "stdafx.h"
#include "site_helpers.h"
#include "dynamic_module.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

typedef void* (*PGET_MODULE_OBJECT_FUNC)(const char* className, uint64_t interfaceTypeId);

std::shared_ptr<ISpxGenericSite> SpxGetSiteFromModule(const char* moduleName)
{
    static std::once_flag initOnce;
    static std::shared_ptr<ISpxGenericSite> site;

    std::call_once(initOnce, [moduleName]() {

        auto module = CSpxDynamicModule::Get(moduleName);
        SPX_THROW_HR_IF(SPXERR_NOT_FOUND, module == nullptr);

        auto pfn = (PGET_MODULE_OBJECT_FUNC)module->GetModuleProcAddress("GetModuleObject");
        SPX_THROW_HR_IF(SPXERR_NOT_FOUND, pfn == nullptr);

        auto ptr = static_cast<ISpxGenericSite*>(pfn("CSpxResourceManager", Type<ISpxGenericSite>::Id));
        SPX_THROW_HR_IF(SPXERR_NOT_FOUND, ptr == nullptr);

        site = SpxSharedPtrFromThis<ISpxGenericSite>(ptr);
        });

    return site;
}

std::shared_ptr<ISpxGenericSite> SpxGetSpeechRootSite()
{
#ifdef __linux__
        auto moduleName = "libMicrosoft.CognitiveServices.Speech.core.so";
#elif __MACH__
        auto moduleName = "libMicrosoft.CognitiveServices.Speech.core.dylib";
#else
        auto moduleName = "Microsoft.CognitiveServices.Speech.core.dll";
#endif
        return SpxGetSiteFromModule(moduleName);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
