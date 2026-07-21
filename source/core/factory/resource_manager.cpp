//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// resource_manager.cpp: Implementation definitions for CSpxResourceManager C++ class
//

#include "stdafx.h"
#include "resource_manager.h"
#include "module_factory.h"
#include "factory_helpers.h"
#include "create_module_object.h"
#include "handle_table.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

std::mutex CSpxResourceManager::registerMutex;
std::list<std::shared_ptr<ISpxObjectFactory>> CSpxResourceManager::m_moduleFactories;

std::map<std::string, PCREATE_MODULE_OBJECT_FUNC>& CSpxResourceManager::GetModuleMap()
{
    static std::map<std::string, PCREATE_MODULE_OBJECT_FUNC> moduleMap;
    return moduleMap;
}

bool CSpxResourceManager::RegisterModule(const std::string& moduleName, PCREATE_MODULE_OBJECT_FUNC pFunc)
{
    std::lock_guard<std::mutex> lock(registerMutex);
    if (GetModuleMap().find(moduleName) == GetModuleMap().end())
    {
        GetModuleMap().insert({moduleName, pFunc});
    }
    return true;
}

void CSpxResourceManager::EnsureLoadExtensionModules()
{
    SPX_DBG_TRACE_FUNCTION();
    AddExtensionModules(m_moduleFactories);
}

CSpxResourceManager::CSpxResourceManager()
{
    SPX_DBG_TRACE_FUNCTION();
    EnableDefaultMemoryLogging();

    // **IMPORTANT**: Do NOT change the order in which module factories are added here!!!
    //
    //   They will be searched in order for objects to create (See ::CreateObject).
    //   Changing the order will have adverse side effects on the intended behavior.
    //
    //   FOR EXAMPLE: CSpxResourceManager intentionally searches for mock objects first.
    //                This allows "at runtime testing".

#ifdef __linux__
    // First, search mocks
    AddMockModules();

    // Second, the known extension modules.
    AddExtensionModules(m_moduleFactories);

    // Third, search inside *this* dynamic module.
    m_moduleFactories.push_back(CSpxModuleFactory::Get("carbon", IntraAssemblyCreateModuleObject));
#elif __MACH__ || defined(EMSCRIPTEN)
    // N.B. dynamic loading of libraries during runtime is not allowed for iOS apps by the App Store.
    // Emscripten dynamic loading is not stable yet and currently not recommended.
    // https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md

    // First, search mocks
    AddMockModules();

    // Second, the known extension modules.
    AddExtensionModules(m_moduleFactories);

    // Third, search inside *this* dynamic module.
    m_moduleFactories.push_back(CSpxModuleFactory::Get("carbon", IntraAssemblyCreateModuleObject));

    for (auto const& adapter : GetModuleMap())
    {
        m_moduleFactories.push_back(CSpxModuleFactory::Get(adapter.first, adapter.second));
    }
#else
    // Note: due to new naming, removing any carbon prefix in name
    // Note: due to dots in filenames, MUST append .dll suffix!

    // First, search mocks
    AddMockModules();

    // Second, the known extension modules.
    AddExtensionModules(m_moduleFactories);

    // Third, search inside *this* dynamic module.
    m_moduleFactories.push_back(CSpxModuleFactory::Get("carbon", IntraAssemblyCreateModuleObject));
#endif
}

CSpxResourceManager::~CSpxResourceManager()
{
    SPX_TRACE_FUNCTION();
    {
        std::lock_guard<std::mutex> lock(m_platformInitMutex);
        for (auto& teardownFunction : m_platformTeardownFunctions)
        {
            SPX_TRACE_INFO("Calling HTTP platform teardown function.");
            teardownFunction();
        }
        m_platformTeardownFunctions.clear();
    }

    SPX_TRACE_INFO("CSpxResourceManager destroyed.");
}

void CSpxResourceManager::AddMockModules()
{
    SPX_DBG_TRACE_FUNCTION();
    std::shared_ptr<ISpxObjectFactory> factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "carbon-mock" _LIB_EXT_);
    factory ? m_moduleFactories.push_back(factory) : void();
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "carbon-tts-mock" _LIB_EXT_);
    factory ? m_moduleFactories.push_back(factory) : void();
}

void* CSpxResourceManager::CreateObject(const char* className, uint64_t typeId)
{
    EnableDefaultFileLogging();

    // Loop through each of our module factories, and see if they can create the object.
    //
    // If more than one module factory can create the object, we'll use the instance
    // from the first module factory that can create it. This enables "mocking" and
    // general "replacement" following the order in which the module factories are
    // added into the module factory list (see ctor...)

    for (auto factory : m_moduleFactories)
    {
        auto obj = factory->CreateObject(className, typeId);
        if (obj != nullptr)
        {
            SPX_DBG_TRACE_VERBOSE("Created '%s' as '%lu'", className, (unsigned long)typeId);
            return obj;
        }
    }

    SPX_TRACE_WARNING(
        "Failed to create '%s' as '%lu'. Are all required extension libraries loaded?",
        className,
        (unsigned long)typeId);
    return nullptr;
}

void CSpxResourceManager::EnableDefaultMemoryLogging()
{
    auto logging = PAL::SpxGetEnv("AZAC_DIAGNOSTICS_LOG").GetOr("");
    auto logFile = PAL::SpxGetEnv("AZAC_DIAGNOSTICS_LOG_FILE").GetOr("");
    auto logFileOk = logFile.size() > 0;

    auto enableMemoryLogging = logging.find("memory") != logging.npos;
    if (enableMemoryLogging)
    {
        diagnostics_log_memory_start_logging();
        diagnostics_log_memory_dump_on_exit(logFile.c_str(), "ONEXIT", false, !logFileOk);
    }
}

void CSpxResourceManager::EnableDefaultFileLogging()
{
    static bool once = false;
    if (once) return;
    once = true;

    auto logging = PAL::SpxGetEnv("AZAC_DIAGNOSTICS_LOG").GetOr("");
    auto logFile = PAL::SpxGetEnv("AZAC_DIAGNOSTICS_LOG_FILE").GetOr("");
    auto logFileOk = logFile.size() > 0;

    auto enableFileLogging = logging.find("file") != logging.npos;
    if (enableFileLogging && logFileOk)
    {
        Set(PropertyId::Speech_LogFilename, logFile.c_str());
        diagnostics_log_start_logging(AZAC_HANDLE_RESERVED1, nullptr);
    }
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
