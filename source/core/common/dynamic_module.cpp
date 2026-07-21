//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// dynamic_module.cpp: Implementation definitions for CSpxDynamicModule C++ class
//

#include "stdafx.h"

#include "dynamic_module.h"
#include "exception.h"
#include "string_utils.h"

#if _MSC_VER
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

std::unique_ptr<CSpxDynamicModule> CSpxDynamicModule::Get(const std::string& filename)
{
    return std::make_unique<CSpxDynamicModule>(filename, NoObject());
}

CSpxDynamicModule::CSpxDynamicModule(const std::string& filename, NoObject)
{
    m_filename = filename;
}

CSpxDynamicModule::SPX_MODULE_FUNC CSpxDynamicModule::GetModuleProcAddress(const std::string& procname)
{
    return GetModuleFunctionPointer(m_filename, procname);
}

CSpxDynamicModule::SPX_MODULE_FUNC CSpxDynamicModule::GetModuleFunctionPointer(
                                                            const std::string& filename,
                                                            const std::string& procname)
{
#if _MSC_VER
    HMODULE handle = GetLibraryHandle(filename);

    if (handle != NULL)
    {
        return (SPX_MODULE_FUNC)GetProcAddress(handle, procname.c_str());
    }
#elif defined(EMSCRIPTEN)   // Emscripten does not need dlopen
    UNUSED(filename);
    UNUSED(procname);
#else
    SPX_TRACE_VERBOSE("Loading '%s'", filename.c_str());
    void* handle = dlopen(filename.c_str(), RTLD_LOCAL | RTLD_LAZY);
    SPX_TRACE_VERBOSE_IF(handle != NULL, "dlopen('%s') returned non-NULL", filename.c_str());
    SPX_TRACE_VERBOSE_IF(handle == NULL, "dlopen('%s') returned NULL: %s", filename.c_str(), dlerror());

    if (handle != NULL)
    {
        auto pfn = (SPX_MODULE_FUNC)dlsym(handle, procname.c_str());
        SPX_TRACE_VERBOSE_IF(pfn != NULL, "dlsym('%s') returned non-NULL", procname.c_str());
        SPX_TRACE_VERBOSE_IF(pfn == nullptr, "dlsym('%s') returned NULL: %s", procname.c_str(),  dlerror());

        if (pfn == nullptr)
        {
            SPX_TRACE_VERBOSE("dlsym('%s') returned NULL: ... thus ... using libMicrosoft.CognitiveServices.Speech.so!%s directly", procname.c_str(), procname.c_str());
            std::string msg = "can't find '" + procname + "' from " + filename;
            ThrowRuntimeError(msg);
        }

        return pfn;
    }
#endif

    return nullptr;
}

#if _MSC_VER
HMODULE CSpxDynamicModule::GetLibraryHandle(const std::string& filename)
{
    std::vector<wchar_t> basePath(_MAX_PATH); // zero-initialized
    if (::GetModuleFileNameW((HINSTANCE)&__ImageBase, &basePath[0], _MAX_PATH) != 0)
    {
        wchar_t* lastBackslash = ::wcsrchr(&basePath[0], L'\\');
        if (lastBackslash)
        {
            // Terminate after final backslash
            *(lastBackslash + 1) = L'\0';
        }
    }

    std::wstring wideFilename = PAL::ToWString(filename);
    std::wstring fullPath = std::wstring(&basePath[0]) + wideFilename;

    HMODULE handle = NULL;
    uint32_t length = 0;
    long result = GetPackageFamilyName(GetCurrentProcess(), &length, nullptr);

    if (result == APPMODEL_ERROR_NO_PACKAGE)
    {
        handle = LoadLibraryExW(fullPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (handle == NULL)
        {
#pragma warning(push)
#pragma warning(disable : 4127)
            if (sizeof(void*) == 4)
#pragma warning(pop)
            {
                // If we are running on 32-bit, and all else has failed, try also the .NET Framework AnyCPU support
                // specific directory for x86 binaries.
                std::wstring dotNetFrameworkAnyCpuX86FullPath = std::wstring(&basePath[0]) + L"\\x86\\" + wideFilename;
                handle = LoadLibraryExW(dotNetFrameworkAnyCpuX86FullPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            }
        }
    }
    else // packaged process
    {
        handle = LoadPackagedLibrary(wideFilename.c_str(), 0);
        if (handle == NULL)
        {
            /* Try again in case it is an uncommon UWP (e.g python) */
            handle = LoadLibraryExW(fullPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

        }
    }

    return handle;
}
#endif

} } } } // Microsoft::CognitiveServices::Speech::Impl
