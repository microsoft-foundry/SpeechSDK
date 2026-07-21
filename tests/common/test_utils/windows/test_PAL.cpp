//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windef.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <debugapi.h>

#include <regex>
#include <locale>
#include <codecvt>

#include "test_PAL.h"

static void free_ie_proxy_config(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG& config)
{
    if (config.lpszProxy != NULL)
    {
        GlobalFree(config.lpszProxy);
        config.lpszProxy = NULL;
    }

    if (config.lpszAutoConfigUrl != NULL) {
        GlobalFree(config.lpszAutoConfigUrl);
        config.lpszAutoConfigUrl = NULL;
    }

    if (config.lpszProxyBypass != NULL) {
        GlobalFree(config.lpszProxyBypass);
        config.lpszProxyBypass = NULL;
    }
}

bool TestUtils::m_debugFlag = false;

bool TestUtils::IsDebuggerAttached()
{
    return IsDebuggerPresent();
}

bool TestUtils::GetSystemProxy(ProxyServerInfo& info)
{
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyInfo;

    try
    {
        // Retrieve the default proxy configuration.
        if (WinHttpGetIEProxyConfigForCurrentUser(&proxyInfo) == FALSE)
        {
            return false;
        }

        if (proxyInfo.fAutoDetect || proxyInfo.lpszProxy == NULL)
        {
            free_ie_proxy_config(proxyInfo);
            return false;
        }

        std::string proxyList;
        {
            std::wstring proxyListW = proxyInfo.lpszProxy;
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            proxyList = converter.to_bytes(proxyListW);
        }

        // proxy list contains a semicolon or whitespace separated list of proxies
        // e.g. http=http://localhost:8888;https=https://localhost:8888/
        // We want the https one
        std::regex parser("https=(https://)?([^:]+)(:([0-9]+))?");
        std::smatch match;
        if (std::regex_search(proxyList, match, parser) && match.size() > 0)
        {
            info.host = match.str(2);
            info.port = 443;

            if (match.size() >= 5)
            {
                info.port = std::stoi(match.str(4));
            }
        }

        free_ie_proxy_config(proxyInfo);
        return true;
    }
    catch (...)
    {
        free_ie_proxy_config(proxyInfo);
        return false;
    }
}

void TestUtils::SetDebugFlag(bool debugFlag)
{
    m_debugFlag = debugFlag;
}

bool TestUtils::GetDebugFlag()
{
    return m_debugFlag;
}
