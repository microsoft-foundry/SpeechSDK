//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// guid_utils.cpp: Utility classes/functions dealing with GUIDs
//

#include "platform.h"

#include <string_utils.h>
#include <sstream>
#include <tuple>

#if defined(_MSC_VER)
#include <Windows.h>
#include <VersionHelpers.h>

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP)
#include <winrt/base.h>
#include <winrt/Windows.System.Profile.h>
#endif

#elif defined(__linux__)
#include <sys/utsname.h>
#ifdef __has_include
#if __has_include(<gnu/libc-version.h>)
#include <gnu/libc-version.h>
#define HAVE_GNU_LIBC_VERSION 1
#endif
#endif
#elif defined(__MACH__)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

// The following snippet comes from
// https://stackoverflow.com/questions/281818/unmangling-the-result-of-stdtype-infoname

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
#endif

#if defined(EMSCRIPTEN)
#include <emscripten/version.h>
#endif

namespace PAL {

#ifdef __GNUG__
std::string demangle(const char* name) {

int status = -4; // some arbitrary value to eliminate the compiler warning
std::unique_ptr<char, void(*)(void*)> res{
    abi::__cxa_demangle(name, NULL, NULL, &status),
    std::free
};

return (status == 0) ? res.get() : name;
}

#else

// does nothing if not g++
std::string demangle(const char* name) {
    return name;
}

#endif

#if defined(_MSC_VER)

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) // Desktop

OperatingSystemInfo getOperatingSystem()
{
    OperatingSystemInfo result { "Windows", "unknown", "unknown" };

    if (IsWindows10OrGreater())
    {
        result.version = "10";
    }
    else if (IsWindows8Point1OrGreater())
    {
        result.version = "8.1";
    }
    else if (IsWindows8OrGreater())
    {
        // Note: this will also be returned if Windows shims the version.
        result.version = "8";
    }

    if (IsWindowsServer())
    {
        result.name = "Server";
    }
    else
    {
        result.name = "Client";
    }
    return result;
}

#else // Windows Store WinRT app

OperatingSystemInfo getOperatingSystem()
{
    OperatingSystemInfo result { "Windows UWP", "unknown", "unknown" };

    try
    {
        auto versionInfo = winrt::Windows::System::Profile::AnalyticsInfo::VersionInfo();
        result.name = winrt::to_string(versionInfo.DeviceFamily());
        uint64_t versionCode = std::strtoull(winrt::to_string(versionInfo.DeviceFamilyVersion()).c_str(), nullptr, 10);
        std::stringstream versionSs;

        for (int i = 0; i < 4; i++, versionCode <<= 16) {
            versionSs << (versionCode >> 48);
            if (i < 3) {
                versionSs << '.';
            }
        }
        result.version = versionSs.str();
    }
    catch (winrt::hresult_error const& /*ex*/)
    {
        // Ignore exceptions thrown via WinRT APIs.
    }
    return result;
}

#endif

#elif defined(__ANDROID__) || defined(ANDROID)

#include <sys/system_properties.h>

OperatingSystemInfo getOperatingSystem()
{
    OperatingSystemInfo result { "Linux; Android", "unknown", "unknown" };
    char prop_str[PROP_VALUE_MAX];
    std::stringstream nameSs;
    if (__system_property_get("ro.build.version.release", prop_str)) {
        nameSs << "Android " << prop_str;
        result.version = prop_str;
    }
    if (__system_property_get("ro.build.version.sdk", prop_str)) {
        nameSs << " API " << prop_str;
    }
    if (__system_property_get("ro.product.cpu.abi", prop_str)) {
        nameSs << " " << prop_str;
    }
#if __ANDROID_API__ >= 26
    std::string propertyValue;

    const prop_info *propertyInfo = __system_property_find("ro.build.fingerprint");
    if (propertyInfo != nullptr)
    {
        __system_property_read_callback(
            propertyInfo,
            [](void *cookie, const char *, const char *value, unsigned) {
                auto propertyValue = reinterpret_cast<std::string *>(cookie);
                *propertyValue     = value;
            },
            &propertyValue);
    }

    nameSs << " " << propertyValue;
#else
    // ro.build.fingerprint is too long in some devices.
    // We should use __system_property_read_callback() when we target API level 26 or higher.
    // Otherwise, we can workaround by constructing the fingerprint from other properties.
    // Refer to https://cs.android.com/android/platform/superproject/+/master:bootable/recovery/updater/build_info.cpp;l=108
    if (__system_property_get("ro.product.brand", prop_str)) {
        nameSs << " " << prop_str;
    }
    if (__system_property_get("ro.product.name", prop_str)) {
        nameSs << "/" << prop_str;
    }
    if (__system_property_get("ro.product.device", prop_str)) {
        nameSs << "/" << prop_str;
    }
    if (__system_property_get("ro.build.version.release", prop_str)) {
        nameSs << ":" << prop_str;
    }
    if (__system_property_get("ro.build.id", prop_str)) {
        nameSs << "/" << prop_str;
    }
    if (__system_property_get("ro.build.version.incremental", prop_str)) {
        nameSs << "/" << prop_str;
    }
    if (__system_property_get("ro.build.type", prop_str)) {
        nameSs << ":" << prop_str;
    }
    if (__system_property_get("ro.build.tags", prop_str)) {
        nameSs << "/" << prop_str;
    }
#endif

    result.name = nameSs.str();
    return result;
}

#elif defined(__linux__)

OperatingSystemInfo getOperatingSystem()
{
    OperatingSystemInfo result { "Linux", "unknown", "unknown" };
    struct utsname u;
    std::stringstream nameSs;
    if (uname(&u) == 0)
    {
        result.platform = u.sysname;
        nameSs << u.sysname << " " << u.release << " " << u.version << " " << u.machine; // "uname -s -r -v -m"

#if defined(HAVE_GNU_LIBC_VERSION)
        const char* glibcVersion = gnu_get_libc_version();
        if (glibcVersion != nullptr)
        {
            nameSs << " glibc/" << glibcVersion;
        }
#endif

        result.name = nameSs.str();
        result.version = u.release;
    }
    return result;
}

#elif defined(__MACH__)

OperatingSystemInfo getOperatingSystem()
{
    int mib[] = { CTL_KERN, KERN_OSRELEASE };
    size_t len;
    sysctl(mib, 2, NULL, &len, NULL, 0);
    auto kernOsRelease = std::make_unique<char[]>(len);
    sysctl(mib, 2, kernOsRelease.get(), &len, NULL, 0);
    return {"Darwin", "Darwin", kernOsRelease.get()};
}

#elif defined(EMSCRIPTEN)

OperatingSystemInfo getOperatingSystem()
{
    std::stringstream versionSs;
    versionSs << __EMSCRIPTEN_major__ << "." << __EMSCRIPTEN_minor__ << "." << __EMSCRIPTEN_tiny__;
    return { "WebAssembly", "Emscripten", versionSs.str() };
}

#endif

Maybe<std::string> SpxGetEnv(const char* name)
{
#if defined(_MSC_VER)
        size_t size = 0;
        char buffer[100];
        getenv_s(&size, nullptr, 0, name);
        if (size > 0 && size < sizeof(buffer))
        {
            getenv_s(&size, buffer, size, name);
            return std::string{ buffer };
        }
#else
        const char* value = getenv(name);
        if (value != nullptr)
        {
            return std::string{ value };
        }
#endif
    return nullptr;
}

} // PAL
