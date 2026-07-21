//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <array>
#include <memory>
#include "azure/core/http/http.hpp"
#include "azure/core/http/transport.hpp"
#include "http_request.h"
#include "http_response.h"
#include "http_exception.h"
#include "http_method_helpers.h"
#include "default_http_error_handler.h"

#ifdef BUILD_TRANSPORT_WINHTTP_ADAPTER
#include "azure/core/http/win_http_transport.hpp"
#else
#include "azure/core/http/curl_transport.hpp"
#endif

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    using namespace Azure::Core;

    /* NOTE:
     * Ideally I'd like to use the NullBodyStream directly from Azure core but this is "internal" only
     * for some reason and so not directly accessible when using vcpkg. This is just a copy of that class
     */
    class NullBodyStream final : public IO::BodyStream
    {
    private:
        size_t OnRead(uint8_t* buffer, size_t count, Azure::Core::Context const& context) override
        {
            (void)context;
            (void)buffer;
            (void)count;
            return 0;
        }

    public:
        explicit NullBodyStream() {}
        int64_t Length() const override { return 0; }
        void Rewind() override {}
        static BodyStream* GetNullBodyStream()
        {
            static NullBodyStream instance;
            return &instance;
        }
    };

    static Http::HttpMethod ToAzureHttpMethod(HttpMethod method)
    {
        return Http::HttpMethod(EnumHelpers::ToString(method));
    }

    static std::string GenerateProxyString(const ProxyServerInfo& proxyToUse)
    {
        std::string hostPort;
        hostPort.reserve(proxyToUse.host.size() + 6); // 6 is for : and max port value length
        hostPort += proxyToUse.host;
        hostPort += ':';
        hostPort += std::to_string(proxyToUse.port);

#ifdef BUILD_TRANSPORT_WINHTTP_ADAPTER
        // The proxy string should be in the following format:
        // http=127.0.0.1:8888;https=127.0.0.1:8888
        std::string proxy("http=");
        proxy.reserve(hostPort.size() + 12); // 12 to account for schemes
        proxy += hostPort;
        proxy += ";https=";
        proxy += hostPort;

        return proxy;
#else
        return hostPort;
#endif
    }

    static std::unique_ptr<Http::HttpTransport> CreateTransport(const IHttpEndpointInfo& endpoint)
    {
        auto proxy = endpoint.Proxy();

        // TODO
        // - How do you specify proxies?
        //      Can't seem to a find a way to do this for the WinHttpTransport
        // - How do you set the various SSL certificate values?
        //      Can't seem to find a way to do this for WinHttpTransport
        //      For lib curl, we usually pass a base64 certificate value. However libcurl expects a path to a cert. Should we write out to some temp file then??
        // - How do you enable CRL checks for the WinHttpTransport?

#ifdef BUILD_TRANSPORT_WINHTTP_ADAPTER

        Http::WinHttpTransportOptions options;

        if (!proxy.host.empty())
        {
            options.ProxyInformation = GenerateProxyString(proxy);
            if (!proxy.username.empty()) options.ProxyUserName = proxy.username;
            if (!proxy.password.empty()) options.ProxyPassword = proxy.password;
        }

        if (endpoint.IsSecure())
        {
            options.EnableCertificateRevocationListCheck = !endpoint.DisableCrlChecks();
            options.IgnoreUnknownCertificateAuthority = endpoint.DisableDefaultVerifyPaths();

            auto singleTrustedCert = endpoint.SingleTrustedCertificate();
            if (!singleTrustedCert.empty())
            {
                options.ExpectedTlsRootCertificates.push_back(singleTrustedCert);

            }

            // Currently the ContinueOnCrlDownloadFailure() and MaxCRLDownloadSizeOption() are not used
            // and/or not supported
        }

        return std::make_unique<Http::WinHttpTransport>(options);

#else

        Http::CurlTransportOptions options;
        if (!proxy.host.empty())
        {
            options.Proxy = GenerateProxyString(proxy);
            if (!proxy.username.empty()) options.ProxyUsername = proxy.username;
            if (!proxy.password.empty()) options.ProxyPassword = proxy.password;
        }

        if (endpoint.IsSecure())
        {
            options.SslOptions.EnableCertificateRevocationListCheck = !endpoint.DisableCrlChecks();
            options.SslOptions.AllowFailedCrlRetrieval = endpoint.ContinueOnCrlDownloadFailure();
            options.SslVerifyPeer = !endpoint.DisableDefaultVerifyPaths();

            auto singleTrustedCert = endpoint.SingleTrustedCertificate();
            if (!singleTrustedCert.empty())
            {
                options.SslOptions.PemEncodedExpectedRootCertificates = singleTrustedCert;

            }

            // Currently MaxCRLDownloadSizeOption() are not used and/or not supported
        }

        return std::make_unique<Http::CurlTransport>(options);

#endif
    }

    CSpxHttpRequest::CSpxHttpRequest()
    {
        SPX_TRACE_FUNCTION("CSpxHttpRequest()");
    }

    CSpxHttpRequest::~CSpxHttpRequest()
    {
        SPX_TRACE_FUNCTION("~CSpxHttpRequest()");
    }

    static void CreateAndSendRequest(const HttpMethod method, const IHttpEndpointInfo& endpoint, const uint8_t* content, const size_t contentSize,
        Context& context,
        ISpxHttpErrorHandler::ConstPtr& handler,
        std::unique_ptr<Http::HttpTransport>& transport,
        std::unique_ptr<Http::RawResponse>& response)
    {
        std::unique_ptr<IO::MemoryBodyStream> stream;
        if (content != nullptr)
        {
            stream = std::make_unique<IO::MemoryBodyStream>(content, contentSize);
        }

        auto parsedMethod = ToAzureHttpMethod(method);
        Azure::Core::Url parsedUrl(endpoint.EndpointUrl());

        // NOTE:
        // For some reason, you can't set the shouldBufferResponse flag if you send an HTTP request
        // with a body. For consistency force the value to true when not using the body
        Http::Request request = stream
            ? Http::Request(parsedMethod, parsedUrl, stream.get())
            : Http::Request(parsedMethod, parsedUrl, true);

        for (const auto& header : endpoint.Headers())
        {
            request.SetHeader(header.first, header.second);
        }

        // TODO
        // Can/should we be re-using the HttpTransport instance here?

        handler = handler ? handler : GetDefaultHttpErrorHandler();

        try
        {
            transport = CreateTransport(endpoint);
            response = transport->Send(request, context);
        }
        catch (const std::exception& ex)
        {
            handler->HandleSendError(method, &endpoint, ex.what());
        }
        catch (...)
        {
            handler->HandleSendError(method, &endpoint, "Unknown error occurred.");
        }
    }

    std::unique_ptr<ISpxHttpResponse> CSpxHttpRequest::SendRequest(
        HttpMethod method, const IHttpEndpointInfo& endpoint, const uint8_t* content, size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler)
    {
        std::unique_ptr<Http::HttpTransport> transport;
        std::unique_ptr<Http::RawResponse> response;
        Context context;
        ISpxHttpErrorHandler::ConstPtr handler = errorHandler;

        CreateAndSendRequest(method, endpoint, content, contentSize,
            context, handler, transport, response);

        return std::make_unique<CSpxHttpResponse>(std::move(transport), method, endpoint, std::move(response), handler);
    }

    std::unique_ptr<ISpxHttpResponse> CSpxHttpRequest::SendRequestStreamResponse(
        HttpMethod method, const IHttpEndpointInfo& endpoint, StreamedResponseDataHandler&& onDataCallback, const uint8_t* content, size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, onDataCallback == nullptr);

        std::unique_ptr<Http::HttpTransport> transport;
        std::unique_ptr<Http::RawResponse> response;
        Context context;
        ISpxHttpErrorHandler::ConstPtr handler = errorHandler;

        CreateAndSendRequest(method, endpoint, content, contentSize,
            context, handler, transport, response);

        // In the Azure C Shared world, we don't return from this function until we are done reading all of
        // of the response from the service, and have called onDataCallback as many times as needed. Let's
        // mimic that behaviour here
        try
        {
            auto stream = response->ExtractBodyStream();
            std::array<uint8_t, 1024> buffer;

            while (true)
            {
                size_t numRead = stream->Read(buffer.data(), buffer.size(), context);
                if (numRead == 0)
                {
                    // we are done reading
                    break;
                }

                onDataCallback(buffer.data(), numRead);
            }
        }
        catch (const std::exception& ex)
        {
            handler->HandleSendError(method, &endpoint, std::string("Failed while reading response body. Details: ") + ex.what());
        }

        return std::make_unique<CSpxHttpResponse>(std::move(transport), method, endpoint, std::move(response), handler);
    }

} } } }
