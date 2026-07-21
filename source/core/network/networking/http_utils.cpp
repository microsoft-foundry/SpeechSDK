//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <stdio.h>
#include "property_id_2_name_map.h"
#include "http_utils.h"
#include "string_utils.h"
#include "error_info.h"
#include "http_endpoint_info.h"
#include "http_platform.h"
#include "spx_build_information.h"
#include "network/usp/header_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    using StringUtils = PAL::StringUtils;

    static constexpr auto HTTP = "http://";
    static constexpr auto HTTPS = "https://";
    static constexpr auto WS = "ws://";
    static constexpr auto WSS = "wss://";
    static constexpr auto FILE = "file://";
    static constexpr auto RTSP = "rtsp://";
    static constexpr auto RTSPS = "rtsps://";
    static constexpr auto HEX_CHARS = "0123456789ABCDEF";
    static constexpr auto INVALID_ESCAPED_URL = "Escaped URL string is invalid";
    static constexpr auto INVALID_URL_SCHEME = "URL scheme is invalid";
    static constexpr auto MISSING_URL_HOST = "URL is missing host";
    static constexpr auto INVALID_URL_PORT = "URL port is invalid";

    Url HttpUtils::ParseUrl(const std::string& rawUrlStr)
    {
        Url parsed;
        auto url = PAL::StringUtils::Trim(rawUrlStr);

        bool success = TryParseScheme(url.c_str(), parsed.scheme);
        if (!success)
        {
            ThrowInvalidArgumentException(INVALID_URL_SCHEME);
        }

        std::string::const_iterator pos = url.begin() + strnlen(SchemePrefix(parsed.scheme), 10);

        auto hostIter = std::find_if(pos, url.cend(), [](char c) {
            return c == '/' || c == '?' || c == ':' || c == '#';
        });

        parsed.host = std::string(pos, hostIter);
        pos = hostIter;

        // for our purposes, we require that the host is set for all schemes other than file://
        if (parsed.scheme != UriScheme::FILE && parsed.host.empty())
        {
            ThrowInvalidArgumentException(MISSING_URL_HOST);
        }

        if (pos != url.end() && *pos == ':')
        {
            auto port_ite = std::find_if_not(pos + 1, url.cend(), [](char c) {
                return std::isdigit(static_cast<unsigned char>(c));
            });

            int portNumber = 0;
            try
            {
                portNumber = std::stoi(std::string(pos + 1, port_ite));
            }
            catch (...)
            {
                ThrowInvalidArgumentException(INVALID_URL_PORT);
            }

            if (static_cast<uint32_t>(portNumber) > std::numeric_limits<uint16_t>::max())
            {
                ThrowInvalidArgumentException(INVALID_URL_PORT);
            }

            parsed.port = portNumber;
            pos = port_ite;
        }

        if (pos != url.end() && (*pos != '/') && (*pos != '?') && (*pos != '#'))
        {
            // only char '/' or '?' or '#' is valid after the port (or the end of the URL)
            ThrowInvalidArgumentException(INVALID_URL_PORT);
        }

        if (pos != url.end() && (*pos == '/'))
        {
            auto pathIter = std::find_if(pos + 1, url.cend(), [](char c) {
                return c == '?' || c == '#';
            });

            parsed.path = std::string(pos + 1, pathIter);
            pos = pathIter;
        }

        if (pos != url.end() && (*pos == '?'))
        {
            auto queryIter = std::find(pos + 1, url.cend(), '#');
            parsed.query = std::string(pos + 1, queryIter);
            pos = queryIter;
        }

        if (pos != url.end() && (*pos == '#'))
        {
            parsed.fragment = std::string(pos + 1, url.cend());
        }

        // If the port was not specified, set it to the default
        if (parsed.port == -1)
        {
            parsed.port = HttpUtils::DefaultPort(parsed.scheme);
        }

        return parsed;
    }

    std::string HttpUtils::UrlEscape(const std::string& value)
    {
        if (value.empty())
        {
            return value;
        }

        std::string escaped;
        for (const char c : value)
        {
            if ((c >= '0' && c <= '9')
                || (c >= 'A' && c <= 'Z')
                || (c >= 'a' && c <= 'z')
                || c == '-'
                || c == '.'
                || c == '_'
                || c == '~')
            {
                escaped += c;
            }
            else
            {
                // escape this character
                uint8_t uc = static_cast<uint8_t>(c);
                escaped += '%';
                escaped += HEX_CHARS[((uc >> 4) & 0x0F)];
                escaped += HEX_CHARS[uc & 0x0F];
            }
        }

        return escaped;
    }

    std::string HttpUtils::UrlUnescape(const std::string& value)
    {
        if (value.empty())
        {
            return value;
        }

        static auto unhex = [](char c) -> uint32_t
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            else if (c >= 'A' && c <= 'F')
            {
                return c - 'A' + 10;
            }
            else if (c >= 'a' && c <= 'f')
            {
                return c - 'a' + 10;
            }
            else
            {
                ThrowInvalidArgumentException(INVALID_ESCAPED_URL);
            }
        };

        uint32_t decoded;

        std::string unescaped;
        for (size_t i = 0; i < value.length(); i++)
        {
            char c = value[i];

            if (c == '+')
            {
                unescaped += ' ';
            }
            else if (c == '%')
            {
                if (i + 2 >= value.length())
                {
                    ThrowInvalidArgumentException(INVALID_ESCAPED_URL);
                }

                char next1 = value[i + 1];
                char next2 = value[i + 2];

                decoded = (unhex(next1) << 4) + unhex(next2);
                if (decoded > std::numeric_limits<uint8_t>::max())
                {
                    ThrowInvalidArgumentException(INVALID_ESCAPED_URL);
                }

                unescaped += static_cast<std::string::value_type>(decoded);
                i += 2;
            }
            else
            {
                unescaped += c;
            }
        }

        return unescaped;
    }

    std::map<std::string, std::vector<std::string>> HttpUtils::ParseQueryString(const std::string & queryString)
    {
        bool first = true;
        std::map<std::string, std::vector<std::string>> parsed;

        std::vector<std::string> parameters = StringUtils::Tokenize(queryString, "&");
        for (const std::string& kvp : parameters)
        {
            auto parts = StringUtils::Tokenize(kvp, "=");
            if (parts.size() > 0 && parts.size() <= 2)
            {
                std::string key = parts[0];
                if (first && !key.empty() && key[0] == '?')
                {
                    key = UrlUnescape(key.substr(1));
                }
                else
                {
                    key = UrlUnescape(key);
                }

                std::string value;
                if (parts.size() > 1)
                {
                    value = UrlUnescape(parts[1]);
                }

                parsed[key].push_back(value);
            }

            first = false;
        }

        return parsed;
    }

    uint16_t HttpUtils::DefaultPort(UriScheme scheme)
    {
        switch (scheme)
        {
        case UriScheme::HTTP:   return 80;
        case UriScheme::HTTPS:  return 443;
        case UriScheme::WS:     return 80;
        case UriScheme::WSS:    return 443;
        case UriScheme::FILE:   return 0;
        case UriScheme::RTSP:   return 80;
        case UriScheme::RTSPS:  return 443;

        // Deliberately don't include default: case here so the GCC compiler will yell at us if we have
        // a scheme missing
        }

        return 0; // should never get here
    }

    const char * HttpUtils::SchemePrefix(UriScheme scheme)
    {
        switch (scheme)
        {
            case UriScheme::HTTP: return HTTP;
            case UriScheme::HTTPS: return HTTPS;
            case UriScheme::WS: return WS;
            case UriScheme::WSS: return WSS;
            case UriScheme::FILE: return FILE;
            case UriScheme::RTSP: return RTSP;
            case UriScheme::RTSPS: return RTSPS;

            // Deliberately don't include default: case here so the GCC compiler will yell at us if we have
            // a scheme missing
        }

        return nullptr; // should never get here
    }

    /// <summary>
    /// Determines the URI scheme from the specified prefix string
    /// </summary>
    /// <param name="str">The null terminated string to examine (e.g. https://)</param>
    /// <param name="scheme>The URI scheme to set</param>
    /// <returns>True if the scheme could be parsed, false otherwise</returns>
    bool HttpUtils::TryParseScheme(const char * str, UriScheme& scheme)
    {
        if (PAL::strnicmp(str, HTTP, strlen(HTTP)) == 0)
        {
            scheme = UriScheme::HTTP;
            return true;
        }
        else if (PAL::strnicmp(str, HTTPS, strlen(HTTPS)) == 0)
        {
            scheme = UriScheme::HTTPS;
            return true;
        }
        else if (PAL::strnicmp(str, WS, strlen(WS)) == 0)
        {
            scheme = UriScheme::WS;
            return true;
        }
        else if (PAL::strnicmp(str, WSS, strlen(WSS)) == 0)
        {
            scheme = UriScheme::WSS;
            return true;
        }
        else if (PAL::strnicmp(str, FILE, strlen(FILE)) == 0)
        {
            scheme = UriScheme::FILE;
            return true;
        }
        else if (PAL::strnicmp(str, RTSP, strlen(RTSP)) == 0)
        {
            scheme = UriScheme::RTSP;
            return true;
        }
        else if (PAL::strnicmp(str, RTSPS, strlen(RTSPS)) == 0)
        {
            scheme = UriScheme::RTSPS;
            return true;
        }

        return false;
    }

    bool HttpUtils::IsSecure(UriScheme scheme)
    {
        switch (scheme)
        {
        case UriScheme::HTTPS:
        case UriScheme::WSS:
        case UriScheme::RTSPS:
            return true;

        case UriScheme::FILE:
        case UriScheme::HTTP:
        case UriScheme::WS:
        case UriScheme::RTSP:
            return false;

        // Deliberately don't include default: case here so the GCC compiler will yell at us if we have
        // a scheme missing
        }

        return false; // should never get here
    }

    /// <summary>
    /// Formats the user agent string, as per https://azure.github.io/azure-sdk/general_azurecore.html
    /// </summary>
    /// <param name="app">The application specific identifier. Only used to identify Speech CLI ("spx"). Otherwise set this to an empty string or nullptr.</param>
    /// <param name="sdk">The SDK string (typically "SpeechSDK")</param>
    /// <param name="language">The programming language in use (e.g. "cpp, "C#", ...)</param>
    /// <param name="version">The SDK version number</param>
    /// <param name="platform">The platform (e.g. Windows 8)</param>
    /// <returns>The user agent string</returns>
    std::string HttpUtils::FormatAzSdkUserAgent(const char* app, const char* sdk, const char* language, const char* version, const char* platform)
    {
        char buffer[4096];
        auto size = sizeof(buffer);
        auto psz = &buffer[0];

        const char* sdk_used =  (sdk && sdk[0]) ? sdk : Microsoft::CognitiveServices::Speech::Impl::BuildInformation::g_SpeechSDKName;
        const char* language_used = (language && language[0]) ? language : "cpp";
        const char* version_used = (version && version[0]) ? version : "";

        if (app != nullptr && app[0] != '\0')
        {
            snprintf(psz, size, "%s ", app);
            auto len = strlen(psz);
            psz += len;
            size -= len;
        }

        if (sdk && strcmp(sdk, BuildInformation::g_SpeechSDKName) != 0)
        {
            // New format
            if (platform && platform[0])
            {
                // To better comply with Azure SDK guidelines, everything after the version should be put in parentheses. See:
                //  https://azure.github.io/azure-sdk/general_azurecore.html
                //  https://www.rfc-editor.org/rfc/rfc7231#page-46
                snprintf(psz, size, "%s-%s/%s (%s)", sdk_used, language_used, version_used, platform);
            }
            else
            {
                snprintf(psz, size, "%s-%s/%s", sdk_used, language_used, version_used);
            }
        }
        else
        {
            const char* platform_used = (platform && platform[0]) ? platform : "";

            // For Speech SDK keep the legacy format for now, as service side tools will need to be updated to support
            // the new format for telemetry-based reports.
            snprintf(psz, size, "%s-%s/%s %s", sdk_used, language_used, version_used, platform_used);
        }

        return buffer;
    }

    size_t HttpUtils::ParseHttpHeaders(const uint8_t* buffer, size_t size, std::map<std::string, std::string>& headers)
    {
        return USP::USPHeaderUtils::Deserialize(buffer, size, headers);
    }

    void HttpUtils::ParseProxyConfig(const ISpxNamedProperties* properties, IHttpEndpointInfo& endpoint, bool validateValues)
    {
        endpoint.BypassProxyFor(PAL::StringUtils::Tokenize(properties->GetOr(PropertyId::SpeechServiceConnection_ProxyHostBypass, ""), ","));

        auto proxyHost = properties->Get(PropertyId::SpeechServiceConnection_ProxyHostName);
        if (!proxyHost || proxyHost.GetOr("").empty())
        {
            return;
        }

        auto proxyPort = properties->Get<int>(PropertyId::SpeechServiceConnection_ProxyPort);
        if (!proxyPort || !HttpUtils::IsValidPort(proxyPort.GetOr(-1)))
        {
            if (validateValues)
            {
                ThrowInvalidArgumentException("Must specify a valid proxy port if you specified a proxy host");
            }
            else
            {
                return;
            }
        }

        auto proxyUsername = properties->Get(PropertyId::SpeechServiceConnection_ProxyUserName);
        auto proxyPassword = properties->Get(PropertyId::SpeechServiceConnection_ProxyPassword);
        if (proxyUsername ^ proxyPassword)
        {
            if (validateValues)
            {
                ThrowInvalidArgumentException("You must either specify both a proxy username and proxy password, or neither value");
            }
            else
            {
                return;
            }
        }

        ProxyServerInfo proxy
        {
            proxyHost.Get(),
            proxyPort.Get(),
            proxyUsername.GetOr(""),
            proxyPassword.GetOr("")
        };

        endpoint.Proxy(&proxy);
    }

    void HttpUtils::ParseSSLConfig(const ISpxNamedProperties* properties, IHttpEndpointInfo& endpoint)
    {
        UNUSED(endpoint);
        if (properties == nullptr)
        {
            return;
        }

        // N.B. the names of the options below have been shared with a customer. Do not change them without consulting with them.
        auto maybeSingleTrustedCert = properties->Get("OPENSSL_SINGLE_TRUSTED_CERT");
        if (maybeSingleTrustedCert)
        {
            endpoint
                .DisableDefaultVerifyPaths(true)
                .SingleTrustedCertificate(maybeSingleTrustedCert.Get());
        }

        endpoint.MaxCRLDownloadSizeOption(properties->GetOr<int>("CONFIG_MAX_CRL_SIZE_KB", MAX_CRL_SIZE_DEFAULT));

        bool disableCrlCheck = properties->GetOr<bool>("OPENSSL_DISABLE_CRL_CHECK", true);
        bool singleTrustedCertCrlCheck = properties->GetOr<bool>("OPENSSL_SINGLE_TRUSTED_CERT_CRL_CHECK", true);
        endpoint.DisableCrlChecks(disableCrlCheck || (maybeSingleTrustedCert && !singleTrustedCertCrlCheck));

        endpoint.ContinueOnCrlDownloadFailure(properties->GetOr<bool>("OPENSSL_CONTINUE_ON_CRL_DOWNLOAD_FAILURE", true));
    }

} } } }
