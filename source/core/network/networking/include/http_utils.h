//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <interfaces/proxy_server_info.h>
#include <interfaces/i_http_endpoint_info.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

#define MAX_CRL_SIZE_DEFAULT 100 * 1024

class ISpxNamedProperties;

/// <summary>
/// The parts of a URL
/// </summary>
struct Url
{
    /// <summary>
    /// The scheme of the URL
    /// </summary>
    UriScheme scheme { UriScheme::HTTPS };
    /// <summary>
    /// The host name (e.g. westus.stt.speech.microsoft.com)
    /// </summary>
    std::string host;
    /// <summary>
    /// The port you are connecting to
    /// </summary>
    int port{ -1 };
    /// <summary>
    /// The path
    /// </summary>
    std::string path;
    /// <summary>
    /// The query string
    /// </summary>
    std::string query;
    /// <summary>
    /// The fragment string
    /// </summary>
    std::string fragment;
};


/// <summary>
/// Utilities for working with HTTP requests
/// </summary>
class HttpUtils
{
public:
    /// <summary>
    /// Parses a URL into its parts
    /// </summary>
    /// <param name="urlStr">The URL to parse</param>
    /// <returns>The parsed URL</returns>
    /// <exception cref="ExceptionWithCallStack">If the URL could not be parsed or is not supported</exception>
    static Url ParseUrl(const std::string& urlStr);

    /// <summary>
    /// URL escapes a value. So for example "Hello? how are you" would become
    /// "Hello%3f%20how%20are%20you"
    /// </summary>
    /// <param name="value">The value to escape</param>
    /// <returns>The escaped value</returns>
    static std::string UrlEscape(const std::string& value);

    /// <summary>
    /// URL unescapes the string. For example "Test%3FYes%3Dtrue" would become
    /// "Test?Yes=true"
    /// </summary>
    /// <param name="value">The value to unescape</param>
    /// <returns>The unescaped value</returns>
    static std::string UrlUnescape(const std::string& value);

    /// <summary>
    /// Parses the query string into name value(s) pairs
    /// </summary>
    /// <param name="queryString">The query string to parse</param>
    /// <returns>The name value(s) pairs</returns>
    static std::map<std::string, std::vector<std::string>> ParseQueryString(const std::string& queryString);

    /// <summary>
    /// Checks whether or not the specified port is valid
    /// </summary>
    /// <param name="port">The port to validate</param>
    static bool IsValidPort(int port)
    {
        return port > 0 && port <= 65535;
    }

    /// <summary>
    /// Gets the default port to be used for the specified scheme
    /// </summary>
    /// <param name="scheme">The URI scheme</param>
    /// <returns>The default port to use</returns>
    static uint16_t DefaultPort(UriScheme scheme);

    /// <summary>
    /// Gets the prefix to use for the URI scheme e.g. https://
    /// </summary>
    /// <param name="scheme">The URI scheme</param>
    /// <returns>The scheme prefix to use</returns>
    static const char * SchemePrefix(UriScheme scheme);

    /// <summary>
    /// Determines the URI scheme from the specified prefix string
    /// </summary>
    /// <param name="str">The null terminated string to examine (e.g. https://)</param>
    /// <param name="scheme>The URI scheme to set</param>
    /// <returns>True if the scheme could be parsed, false otherwise</returns>
    static bool TryParseScheme(const char * str, UriScheme& scheme);

    /// <summary>
    /// Gets whether or not the URI scheme is secure
    /// </summary>
    /// <param name="scheme">The URI scheme to check</param>
    /// <returns>True if the scheme is secure, false if it is not</returns>
    static bool IsSecure(UriScheme scheme);

    /// <summary>
    /// Formats the user agent string, as per https://azure.github.io/azure-sdk/general_azurecore.html:
    /// </summary>
    /// <param name="app">The application specific identifier</param>
    /// <param name="sdk">The SDK string ("SpeechSDK" typically)</param>
    /// <param name="language">The programming language in use (e.g. "cpp, "C#", ...)</param>
    /// <param name="version">The SDK version number</param>
    /// <param name="platform">The platform (e.g. Windows 8)</param>
    /// <returns>The user agent string</returns>
    static std::string FormatAzSdkUserAgent(const char* app, const char* sdk, const char* language, const char* version, const char* platform);

    /// <summary>
    /// Parses HTTP headers from the specified buffer. It interprets the first : as the separator between the header
    /// name, and the value
    /// </summary>
    /// <param name="buffer">The buffer of data to parse from</param>
    /// <param name="size">The number of bytes in the header</param>
    /// <param name="headers">The headers map to populate</param>
    /// <returns>How many bytes the header data was. This can be used as an offset into the buffer to determine
    /// where the HTTP payload starts</returns>
    static size_t ParseHttpHeaders(const uint8_t* buffer, size_t size, std::map<std::string, std::string>& headers);

    /// <summary>
    /// Helper method to parse the proxy information, and proxy bypass list from the named properties.
    /// </summary>
    /// <param name="endpoint">The endpoint to configure</param>
    /// <param name="properties">The named properties to parse</param>
    /// <param name="validateValues">True to throw exceptions on invalid values, false to ignore</param>
    /// <returns>A reference to the http endpoint info for chaining</returns>
    static void ParseProxyConfig(const ISpxNamedProperties* properties, IHttpEndpointInfo& endpoint, bool validateValues = true);

    /// <summary>
    /// Helper method to parse the SSL related configuration from the named properties. This will configure CRL
    /// checks, max CRL download size, continuing on CRL download failures, as well as single trusted certs
    /// </summary>
    /// <param name="endpoint">The endpoint to configure</param>
    /// <param name="properties">The named properties to parse</param>
    static void ParseSSLConfig(const ISpxNamedProperties* properties, IHttpEndpointInfo& endpoint);
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
