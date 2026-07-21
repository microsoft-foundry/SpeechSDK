//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// dll.cpp : Defines the entry point for the DLL application.


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <TraceLoggingProvider.h>
// forward-declare
TRACELOGGING_DECLARE_PROVIDER(tracingEventProvider);
#endif

#include "stdafx.h"
#include "handle_table.h"
#include "trace_message.h"
#include "create_module_object.h"
#include "string_utils.h"
#include "site_helpers.h"
#include "service_helpers.h"
#include "http_platform.h"

void InitLogging();

using namespace Microsoft::CognitiveServices::Speech::Impl;

#ifdef _MSC_VER

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    UNUSED(hModule);
    UNUSED(lpReserved);
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            InitLogging();
#ifdef _WIN32
            TraceLoggingRegister(tracingEventProvider);
#endif
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
#ifdef _WIN32
            TraceLoggingUnregister(tracingEventProvider);
#endif
            CSpxSharedPtrHandleTableManager::Term(true);
            break;
    }
    return TRUE;
}
#elif !defined(EMSCRIPTEN)  // Emscripten does not need LibLoad/LibUnload

__attribute__((constructor)) static void LibLoad(int argc, char** argv, char** envp)
{
    UNUSED(argc);
    UNUSED(argv);
    UNUSED(envp);
#ifdef __MACH__
    InitLogging();
#endif
}

__attribute__((destructor)) static void LibUnload()
{
    CSpxSharedPtrHandleTableManager::Term(true);
}

#endif

void InitLogging()
{
    PAL::HttpPlatform::SetLoggingFunction(SpxTraceMessage2);
}

SPX_EXTERN_C SPXDLL_EXPORT void* CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    return IntraAssemblyCreateModuleObject(className, interfaceTypeId);
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
