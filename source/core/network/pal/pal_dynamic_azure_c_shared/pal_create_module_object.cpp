//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "factory_helpers.h"
#include "pal_create_module_object.h"
#include "http_platform_impl.h"
#include "dynamic_module.h"
#include <log_utils.h>
#include <trace_message.h>

using namespace Microsoft::CognitiveServices::Speech::Impl;

std::shared_ptr<CSpxDynamicModule> PlatformImpl;
ReaderWriterLock PlatformImplLock;

void* DynamicAzureCShared_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
        static std::once_flag m_initOnce;
        static CSpxDynamicModule::SPX_MODULE_FUNC cmoFunc = nullptr;

        std::call_once(m_initOnce, []()
        {
            if(PlatformImpl == nullptr)
            {
                SPX_TRACE_INFO("Platform not loaded yet, loading.");
                void *maybeRet = nullptr;

                pal_get_platform((void**)&maybeRet);
            }
            if(PlatformImpl == nullptr)
            {
                return;
            }

            cmoFunc = PlatformImpl->GetModuleProcAddress("Pal_CreateModuleObject");
        });
    
    if(cmoFunc == nullptr)
    {
        return nullptr;
    }

    auto cmo_call = (void*(*)(const char*, uint64_t))cmoFunc;
    return cmo_call(className, interfaceTypeId);
}

AZAC_EXTERN_C void* Microsoft::CognitiveServices::Speech::Impl::Pal_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    SPX_FACTORY_MAP_BEGIN()
        SPX_FACTORY_MAP_ENTRY_FUNC(DynamicAzureCShared_CreateModuleObject)
    SPX_FACTORY_MAP_END()
}

IHttpPlatform* GetPlatformFromModule(std::shared_ptr<CSpxDynamicModule> module)
{

    auto get_platform = module->GetModuleProcAddress("pal_get_platform");
    if (get_platform == nullptr)
    {
        SPX_TRACE_INFO("Failed to get PAL method from assembly");
        return nullptr;
    }

    SPX_TRACE_SCOPE("Loaded PAL method, calling.", "PAL Call Complete.");
    auto get_platform_call = (int(*)(void**))get_platform;

    void *maybeRet = nullptr;

    auto success = get_platform_call(&maybeRet);
    if(success != AZAC_ERR_NONE)
    {
        SPX_TRACE_INFO("PAL call failed %d.", success);
        return nullptr;
    }

    IHttpPlatform* httpPlatform = (IHttpPlatform*)maybeRet;
    return httpPlatform;
}

PAL_API pal_get_platform(void** platform)
{
    try
    {
        static std::once_flag m_initOnce;
        static IHttpPlatform* httpPlatform = nullptr;

        std::call_once(m_initOnce, []()
        {
            SPX_TRACE_VERBOSE("Trying to load libssl.so.3");
            std::string assembly;

            auto libssl = dlopen("libssl.so.3", RTLD_LAZY);
            if(libssl != nullptr)
            {
                assembly = _LIB_PREFIX_ "pal_azure_c_shared_openssl3" _LIB_EXT_;    
            }
            else
            {
                SPX_TRACE_INFO("Failed to load OpenSSL3 %s", dlerror());
                assembly = _LIB_PREFIX_ "pal_azure_c_shared" _LIB_EXT_;
            }

            SPX_TRACE_VERBOSE("Creating CSpxDyanmic for %s.", assembly.c_str());
            std::shared_ptr<CSpxDynamicModule> module = CSpxDynamicModule::Get(assembly.c_str());
            if (module == nullptr)
            {
                SPX_TRACE_INFO("Failed to load %s", assembly.c_str());
                httpPlatform = nullptr;
                return;
            }
                
            SPX_TRACE_VERBOSE("Loaded %s, getting PAL method.", assembly.c_str());
            httpPlatform = GetPlatformFromModule(module);

            if(nullptr == httpPlatform)
            {
                SPX_TRACE_INFO("Failed to get PAL method from %s", assembly.c_str());
                return;
            }

            httpPlatform->SetLoggingFunction(SpxTraceMessage2);
/*
            SPX_TRACE_VERBOSE("Initializing %s.", assembly.c_str());
            httpPlatform->Init();
*/    
            PlatformImpl = std::move(module);
            return;
        });

        *platform = httpPlatform;
        return httpPlatform == nullptr ? AZAC_ERR_RUNTIME_ERROR : AZAC_ERR_NONE;
    }
    catch (...)
    {
        return AZAC_ERR_RUNTIME_ERROR;
    }
}
