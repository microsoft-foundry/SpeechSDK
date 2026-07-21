//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <algorithm>
#include <http_endpoint_info.h>
#include <http_utils.h>
#include <spxdebug.h>
#include <sstream>
#include <string_utils.h>
#include <http_platform.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    using StringUtils = PAL::StringUtils;
    using namespace std;


    const size_t MAX_HOSTNAME_LEN = 253;


    HttpEndpointInfo::HttpEndpointInfo() :
        m_url(),
        m_queryParams(),
        m_headers(),
        m_proxy(),
        m_proxyBypass(),
        m_singleTrustedCert(),
        m_disableCrlChecks(false),
        m_continueOnCrlDownloadFailure(false),
        m_disableDefaultVerifyPaths(false),
        m_maxCrlDownloadSizeInKB(0),
        m_underlyingOptions(),
        m_webSocketProtocols()
    {
        m_url.scheme = UriScheme::HTTPS;
        m_url.port = HttpUtils::DefaultPort(m_url.scheme);
        m_url.path = "/";
    }

    HttpEndpointInfo::HttpEndpointInfo(const std::string& endpointUrl) : HttpEndpointInfo()
    {
        EndpointUrl(endpointUrl);
    }

    bool HttpEndpointInfo::IsValid() const
    {
        return !m_url.host.empty()
            && HttpUtils::IsValidPort(m_url.port);
    }

    std::string HttpEndpointInfo::EndpointUrl() const
    {
        if (!IsValid())
        {
            throw std::logic_error("Endpoint is not valid");
        }

        return GenerateEndpointUrl(GeneratePathAndQueryString(GenerateQueryString()));
    }

    IHttpEndpointInfo& HttpEndpointInfo::EndpointUrl(const std::string & endpointUrl)
    {
        Url parsed = HttpUtils::ParseUrl(endpointUrl);

        Scheme(parsed.scheme)
            .Host(parsed.host)
            .Port(parsed.port)
            .Path(parsed.path)
            .QueryString(parsed.query);

        return *this;
    }

    UriScheme HttpEndpointInfo::Scheme() const
    {
        return m_url.scheme;
    }

    IHttpEndpointInfo& HttpEndpointInfo::Scheme(UriScheme scheme)
    {
        auto oldScheme = m_url.scheme;
        auto oldDefaultPort = IsDefaultPort();

        m_url.scheme = scheme;

        // update the port if it was not set, or was set to the default
        if (!HttpUtils::IsValidPort(m_url.port)
            || (oldDefaultPort && oldScheme != scheme))
        {
            m_url.port = HttpUtils::DefaultPort(m_url.scheme);
        }

        return *this;
    }

    bool HttpEndpointInfo::IsSecure() const
    {
        return HttpUtils::IsSecure(m_url.scheme);
    }

    std::string HttpEndpointInfo::Host() const
    {
        return m_url.host;
    }

    IHttpEndpointInfo& HttpEndpointInfo::Host(const std::string & hostname)
    {
        std::string trimmed = StringUtils::Trim(hostname);

        if (trimmed.empty())
        {
            throw std::invalid_argument("Host name cannot be empty");
        }
        else if (trimmed.length() > MAX_HOSTNAME_FQDN_LEN)
        {
            throw std::out_of_range("Host name is too long");
        }

        m_url.host = std::move(trimmed);
        return *this;
    }

    int HttpEndpointInfo::Port() const
    {
        return m_url.port < 0 ? -1 : m_url.port;
    }

    IHttpEndpointInfo& HttpEndpointInfo::Port(int port)
    {
        if (!HttpUtils::IsValidPort(port))
        {
            throw std::invalid_argument("Port is not valid");
        }

        m_url.port = port;
        return *this;
    }

    bool HttpEndpointInfo::IsDefaultPort() const
    {
        return m_url.port == HttpUtils::DefaultPort(m_url.scheme);
    }

    std::string HttpEndpointInfo::Path() const
    {
        return m_url.path;
    }

    IHttpEndpointInfo& HttpEndpointInfo::Path(const std::string & path)
    {
        // make sure to add the / at the start if not already there
        std::ostringstream oss;
        oss << '/';

        StringUtils::Trim(path, oss, [](char c, bool atStart) { return atStart && c == '/'; });

        m_url.path = oss.str();
        return *this;
    }

    std::string HttpEndpointInfo::QueryString() const
    {
        return GenerateQueryString();
    }

    IHttpEndpointInfo& HttpEndpointInfo::QueryString(const std::string & queryString)
    {
        m_queryParams.clear();
        m_queryParams = HttpUtils::ParseQueryString(queryString);
        return *this;
    }

    IHttpEndpointInfo& HttpEndpointInfo::AddQueryParameter(const std::string & name, const std::string & value)
    {
        if (name.empty())
        {
            throw std::invalid_argument("Query parameter name cannot be empty");
        }

        m_queryParams[name].push_back(value);
        return *this;
    }

    IHttpEndpointInfo& HttpEndpointInfo::SetQueryParameter(const std::string & name, const::std::string & value)
    {
        if (name.empty())
        {
            throw std::invalid_argument("Query parameter name cannot be empty");
        }

        auto& values = m_queryParams[name];
        values.clear();
        values.push_back(value);
        return *this;
    }

    ProxyServerInfo HttpEndpointInfo::Proxy() const
    {
        const std::string& host = Host();

        // check if the host specified is in the bypass list
        auto found = std::find_if(m_proxyBypass.begin(), m_proxyBypass.end(), [&host](const std::string& other)
        {
            return host.size() == other.size() && PAL::stricmp(host.c_str(), other.c_str()) == 0;
        });

        if (found != m_proxyBypass.end())
        {
            return ProxyServerInfo();
        }

        return m_proxy;
    }

    IHttpEndpointInfo& HttpEndpointInfo::Proxy(const ProxyServerInfo* proxy)
    {
        if (proxy == nullptr)
        {
            m_proxy = ProxyServerInfo();
        }
        else
        {
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, proxy->host.length() > MAX_HOSTNAME_LEN);
            SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !proxy->host.empty() && !HttpUtils::IsValidPort(proxy->port));

            m_proxy = *proxy;
        }

        return *this;
    }

    const std::vector<std::string>& HttpEndpointInfo::BypassProxyFor() const
    {
        return m_proxyBypass;
    }

    IHttpEndpointInfo& HttpEndpointInfo::BypassProxyFor(const std::vector<std::string>& bypass)
    {
        m_proxyBypass.clear();
        for (const std::string& host : bypass)
        {
            auto trimmed = StringUtils::Trim(host);
            if (!trimmed.empty())
            {
                m_proxyBypass.push_back(std::move(trimmed));
            }
        }

        return *this;
    }

    const std::map<std::string, std::string>& HttpEndpointInfo::Headers() const
    {
        return m_headers;
    }

    IHttpEndpointInfo& HttpEndpointInfo::SetHeader(const std::string & name, const std::string & value)
    {
        if (name.empty())
        {
            throw std::invalid_argument("Header name cannot be empty");
        }

        m_headers[name] = value;
        return *this;
    }

    std::string HttpEndpointInfo::WebSocketProtocols() const
    {
        bool first = true;
        std::ostringstream oss;

        for (size_t i = 0; i < m_webSocketProtocols.size(); i++, first = false)
        {
            if (!first)
            {
                oss << ", ";
            }

            oss << m_webSocketProtocols[i];
        }

        return oss.str();
    }

    size_t HttpEndpointInfo::WebSocketProtocolsSize() const
    {
        return m_webSocketProtocols.size();
    }

    IHttpEndpointInfo& HttpEndpointInfo::AddWebSocketProtocol(const std::string & protocol)
    {
        if (protocol.empty())
        {
            throw std::invalid_argument("Web socket protocol cannot be empty");
        }

        auto iter = find(m_webSocketProtocols.begin(), m_webSocketProtocols.end(), protocol);
        if (iter == m_webSocketProtocols.end())
        {
            m_webSocketProtocols.push_back(protocol);
        }

        return *this;
    }

    std::string HttpEndpointInfo::SingleTrustedCertificate() const
    {
        return m_singleTrustedCert;
    }

    IHttpEndpointInfo& HttpEndpointInfo::SingleTrustedCertificate(const std::string & trustedCert)
    {
        m_singleTrustedCert = trustedCert;
        return *this;
    }

    bool HttpEndpointInfo::DisableCrlChecks() const
    {
        return m_disableCrlChecks;
    }

    IHttpEndpointInfo& HttpEndpointInfo::DisableCrlChecks(const bool disableCrlChecks)
    {
        m_disableCrlChecks = disableCrlChecks;
        return *this;
    }

    IHttpEndpointInfo& HttpEndpointInfo::ContinueOnCrlDownloadFailure(const bool continueOnCrlDownloadFailure)
    {
        m_continueOnCrlDownloadFailure = continueOnCrlDownloadFailure;
        return *this;
    }

    bool HttpEndpointInfo::ContinueOnCrlDownloadFailure() const
    {
        return m_continueOnCrlDownloadFailure;
    }

    bool HttpEndpointInfo::DisableDefaultVerifyPaths() const
    {
        return m_disableDefaultVerifyPaths;
    }

    IHttpEndpointInfo& HttpEndpointInfo::DisableDefaultVerifyPaths(const bool disable)
    {
        m_disableDefaultVerifyPaths = disable;
        return *this;
    }

    IHttpEndpointInfo& HttpEndpointInfo::SetUnderlyingOption(const std::string& name, int value)
    {
        if (name.empty())
        {
            throw std::invalid_argument("Option name cannot be empty");
        }

        m_underlyingOptions[name] = value;
        return *this;
    }

    const std::map<std::string, int>& HttpEndpointInfo::UnderlyingOptions() const
    {
        return m_underlyingOptions;
    }

    int HttpEndpointInfo::MaxCRLDownloadSizeOption() const
    {
        return m_maxCrlDownloadSizeInKB;
    }

    IHttpEndpointInfo& HttpEndpointInfo::MaxCRLDownloadSizeOption(int maxCrlDownloadSize)
    {
        m_maxCrlDownloadSizeInKB = maxCrlDownloadSize;
        return *this;
    }

    std::unique_ptr<IHttpEndpointInfo> HttpEndpointInfo::Clone() const
    {
        return std::unique_ptr<IHttpEndpointInfo>(new HttpEndpointInfo(*this));
    }

    std::string HttpEndpointInfo::GenerateQueryString() const
    {
        bool first = true;
        std::ostringstream oss;

        for (auto const& kvp : m_queryParams)
        {
            std::string encodedKey = HttpUtils::UrlEscape(kvp.first);

            const std::vector<std::string> &values = kvp.second;
            for (const std::string &value : values)
            {
                if (first)
                {
                    first = false;
                    oss << "?";
                }
                else
                {
                    oss << "&";
                }

                oss << encodedKey;
                if (!value.empty())
                {
                    oss << "=" << HttpUtils::UrlEscape(value);
                }
            }
        }

        return oss.str();
    }

    std::string HttpEndpointInfo::GeneratePathAndQueryString(const std::string & queryString) const
    {
        std::ostringstream oss;

        if (m_url.path.empty() || m_url.path[0] != '/')
        {
            oss << '/';
        }

        oss << m_url.path;

        if (!queryString.empty() && queryString[0] != '?')
        {
            oss << '?';
        }

        oss << queryString;

        return oss.str();
    }

    std::string HttpEndpointInfo::GenerateEndpointUrl(const std::string & pathAndQueryString) const
    {
        if (!IsValid())
        {
            throw std::logic_error("Endpoint is not valid");
        }

        std::ostringstream oss;
        oss << HttpUtils::SchemePrefix(m_url.scheme)
            << m_url.host;

        if (!IsDefaultPort())
        {
            oss << ':' << m_url.port;
        }

        if (pathAndQueryString.empty() || pathAndQueryString[0] != '/')
        {
            oss << '/';
        }

        oss << pathAndQueryString;

        return oss.str();
    }
}}}} // Microsoft::CognitiveServices::Speech::Impl

