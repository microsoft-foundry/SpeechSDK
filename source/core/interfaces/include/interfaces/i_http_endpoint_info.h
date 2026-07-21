//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <interfaces/proxy_server_info.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// URI schemes
    /// </summary>
    enum class UriScheme
    {
        HTTPS = 0,
        WSS,
        HTTP,
        WS,
        FILE,
        RTSP,
        RTSPS,
    };

    /// <summary>
    /// Defines the parameters for an HTTP endpoint
    /// </summary>
    class IHttpEndpointInfo
    {
    public:
        /// <summary>
        /// Virtual destructor
        /// </summary>
        virtual ~IHttpEndpointInfo() = default;

        /// <summary>
        /// Gets whether or not we have a valid endpoint URL defined.
        /// </summary>
        /// <returns>True if valid, false otherwise</returns>
        virtual bool IsValid() const = 0;

        /// <summary>
        /// Gets the absolute URL of the endpoint
        /// </summary>
        /// <returns>The URL of the endpoint</returns>
        /// <exception cref="std::logic_error">Thrown if we don't have a valid endpoint defined</exception>
        virtual std::string EndpointUrl() const = 0;

        /// <summary>
        /// Sets the absolute URL. This will override all the existing values for the URL including
        /// any set query parameters.
        /// </summary>
        /// <param name="endpointUrl">The absolute endpoint URL to set e.g. https://www.contoso.com/path/subpath?key=value </param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& EndpointUrl(const std::string& endpointUrl) = 0;

        /// <summary>
        /// Gets the HTTP protocol for the endpoint. Please note that this will change the port used
        /// if it is currently set to the defaults.
        /// </summary>
        /// <returns>The HTTP protocol</returns>
        virtual UriScheme Scheme() const = 0;

        /// <summary>
        /// Sets the HTTP protocol for the endpoint
        /// </summary>
        /// <param name="scheme">The URI scheme</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& Scheme(UriScheme scheme) = 0;

        /// <summary>
        /// Gets whether or not the connection should be secure
        /// </summary>
        virtual bool IsSecure() const = 0;

        /// <summary>
        /// Gets the host name only (e.g. www.contoso.com)
        /// </summary>
        virtual std::string Host() const = 0;

        /// <summary>
        /// Sets the host name
        /// </summary>
        /// <param name="hostname">The host name to use e.g. www.contoso.com</param>
        /// <exception cref="std::invalid_argument">If host name is empty</exception>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& Host(const std::string& hostname) = 0;

        /// <summary>
        /// Gets the endpoint port
        /// </summary>
        /// <returns>The current port set, or -1 if no port was set</returns>
        virtual int Port() const = 0;

        /// <summary>
        /// Sets the endpoint port.
        /// </summary>
        /// <param name="port">The port to set</param>
        /// <exception cref="std::invalid_argument">If the port is invalid</exception>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& Port(int port) = 0;

        /// <summary>
        /// Gets whether or not the port specified is the default for the current URI scheme
        /// </summary>
        /// <returns>True if default, false otherwise</returns>
        virtual bool IsDefaultPort() const = 0;

        /// <summary>
        /// Gets the path that the host is listening on (e.g. /sub/other)
        /// </summary>
        virtual std::string Path() const = 0;

        /// <summary>
        /// Sets the path that the host is listening on
        /// </summary>
        /// <param name="path">The path to set.</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& Path(const std::string& path) = 0;

        /// <summary>
        /// Gets the escaped query string to use. If there are query parameters, this will always
        /// start with '?'
        /// </summary>
        virtual std::string QueryString() const = 0;

        /// <summary>
        /// Sets the query string to use
        /// </summary>
        /// <param name="queryString">The query string</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& QueryString(const std::string& queryString) = 0;

        /// <summary>
        /// Adds a query parameter to be included in the upgrade request to the server. You
        /// can set the same name multiple times if you need to. Doing so will result in
        /// the query parameter being set once with comma separated values
        /// </summary>
        /// <param name="name">The query parameter name</param>
        /// <param name="value">The query value</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& AddQueryParameter(const std::string& name, const std::string& value) = 0;

        /// <summary>
        /// Sets the value for query parameter to be included in the upgrade request to the
        /// server. If there were other values defined before, this will overwrite them with
        /// the new single value
        /// </summary>
        /// <param name="name">The query parameter name</param>
        /// <param name="value">The query value</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& SetQueryParameter(const std::string& name, const::std::string& value) = 0;

        /// <summary>
        /// Gets the proxy server to use. If you enabled automatic proxy, this will parse and return
        /// the platform proxy information
        /// </summary>
        virtual ProxyServerInfo Proxy() const = 0;

        /// <summary>
        /// Sets the proxy to use. Calling this will disable automatic proxy
        /// </summary>
        /// <param name="proxy">The proxy configuration to use. Set this to nullptr to disable using a proxy.</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& Proxy(const ProxyServerInfo* proxy) = 0;

        /// <summary>
        /// Sets the proxy to use. Calling this will disable automatic proxy
        /// </summary>
        /// <param name="hostname">The proxy host name to use</param>
        /// <param name="port">The proxy port to use</param>
        /// <param name="username">(Optional) The username to use when authenticating with the proxy</param>
        /// <param name="password">(Optional) The password to use when authenticating with the proxy</param>
        /// <returns></returns>
        virtual IHttpEndpointInfo& Proxy(const std::string& hostname, uint16_t port, const std::string& username = {}, const std::string& password = {})
        {
            ProxyServerInfo proxy{ hostname, (int)port, username, password };
            return Proxy(&proxy);
        }

        /// <summary>
        /// Gets the list of hosts we don't want to use the proxies for
        /// </summary>
        /// <returns>The list of hosts to bypass the proxy for</returns>
        virtual const std::vector<std::string>& BypassProxyFor() const = 0;

        /// <summary>
        /// Sets the list of hosts that we don't want to use the proxies for. This will supersede all other settings.
        /// The host match is done in a case insensitive manner. No wild cards are supported
        /// </summary>
        /// <param name="bypass">The list of hosts</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& BypassProxyFor(const std::vector<std::string>& bypass) = 0;

        /// <summary>
        /// Gets a reference to the headers to include in the request
        /// </summary>
        virtual const std::map<std::string, std::string>& Headers() const = 0;

        /// <summary>
        /// Adds a header to be sent with the HTTP request
        /// </summary>
        /// <param name="name">The name of the header</param>
        /// <param name="value">The header value</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& SetHeader(const std::string& name, const std::string& value) = 0;

        /// <summary>
        /// Gets the comma separated list of web socket protocols to use. This will be empty if none
        /// were set
        /// </summary>
        virtual std::string WebSocketProtocols() const = 0;

        /// <summary>
        /// Gets the number of web socket protocols defined
        /// </summary>
        virtual size_t WebSocketProtocolsSize() const = 0;

        /// <summary>
        /// Adds a web socket protocol to include in the headers when negotiating an upgrade request
        /// with the server. You don't always need to set this
        /// </summary>
        /// <param name="protocol">The protocol to include</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& AddWebSocketProtocol(const std::string& protocol) = 0;

        /// <summary>
        /// Gets the single trusted certificate to use.
        /// </summary>
        /// <returns>The trusted cert, or an empty string if none was set.</returns>
        virtual std::string SingleTrustedCertificate() const = 0;

        /// <summary>
        /// When using OpenSSL only: sets a single trusted cert.
        /// This is meant to be used in a firewall setting with
        /// potential lack of CRLs (particularly on the leaf).
        /// </summary>
        /// <param name="trustedCert">The certificate to trust (PEM format)</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& SingleTrustedCertificate(const std::string& trustedCert) = 0;

        /// <summary>
        /// Sets whether Certificate Revocation List (CRL) checks are done.
        /// </summary>
        /// <param name="disableCrlChecks">True to disable checks, false otherwise</param>
        virtual IHttpEndpointInfo& DisableCrlChecks(const bool disableCrlChecks) = 0;

        /// <summary>
        /// Gets whether or not CRL checks are disabled
        /// </summary>
        /// <returns>True if disabled, false otherwise</returns>
        virtual bool DisableCrlChecks() const = 0;

        /// <summary>
        /// Sets whether or not CRL checks will be skipped if the CRL cannot be downloaded.
        /// </summary>
        /// <param name="continueOnCrlDownloadFailure">True to continue on failure, false otherwise</param>
        virtual IHttpEndpointInfo& ContinueOnCrlDownloadFailure(const bool continueOnCrlDownloadFailure) = 0;

        /// <summary>
        /// Gets whether or not CRL checks will be skipped if the CRL cannot be downloaded.
        /// </summary>
        /// <returns>True if disabled, false otherwise</returns>
        virtual bool ContinueOnCrlDownloadFailure() const = 0;

        /// <summary>
        /// Gets whether or not verification of default paths is disabled
        /// </summary>
        /// <returns>True if disabled, false otherwise</returns>
        virtual bool DisableDefaultVerifyPaths() const = 0;

        /// <summary>
        /// Sets whether or not disable default verify paths
        /// </summary>
        /// <param name="disable">True to disable, false to enable</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& DisableDefaultVerifyPaths(const bool disable) = 0;

        /// <summary>
        /// Gets max CRL download size option.
        /// </summary>
        /// <returns>Max size in mbs</returns>
        virtual int MaxCRLDownloadSizeOption() const = 0;

        /// <summary>
        /// Sets max CRL download size option.
        /// </summary>
        /// <param name="maxCrlDownloadSize">Max size in kbs</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& MaxCRLDownloadSizeOption(int maxCrlDownloadSize) = 0;

        /// <summary>
        /// Sets an underlying IO option.
        /// </summary>
        /// <param name="name">The name of the option</param>
        /// <param name="value">The option value</param>
        /// <returns>A reference to this instance</returns>
        virtual IHttpEndpointInfo& SetUnderlyingOption(const std::string& name, int value) = 0;

        /// <summary>
        /// Gets the underlying IO options
        /// </summary>
        /// <returns>A reference to the underlying options map</returns>
        virtual const std::map<std::string, int>& UnderlyingOptions() const = 0;

        /// <summary>
        /// Creates a copy of the endpoint information
        /// </summary>
        /// <returns>The copy of the endpoint information</returns>
        virtual std::unique_ptr<IHttpEndpointInfo> Clone() const = 0;
    };

}}}}
