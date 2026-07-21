//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// rest_tts_engine_adapter.cpp: Implementation definitions for CSpxRestTtsEngineAdapter C++ class
//

#include "stdafx.h"
#include "synthesis_helper.h"
#include "rest_tts_engine_adapter.h"
#include "create_object_helpers.h"
#include "guid_utils.h"
#include "http_utils.h"
#include "endpoint_utils.h"
#include "service_helpers.h"
#include "shared_ptr_helpers.h"
#include "property_bag_impl.h"
#include "property_id_2_name_map.h"
#include "usp.h"
#include <spx_build_information.h>
#include "error_info.h"
#include "interfaces/ispx_http_request.h"
#include "http_exception.h"
#include "interfaces/ispx_http_transport_factory.h"

#define SPX_DBG_TRACE_REST_TTS 0


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxRestTtsEngineAdapter::CSpxRestTtsEngineAdapter()
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_REST_TTS, __FUNCTION__);
}

CSpxRestTtsEngineAdapter::~CSpxRestTtsEngineAdapter()
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_REST_TTS, __FUNCTION__);
}

void CSpxRestTtsEngineAdapter::Init()
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_REST_TTS, __FUNCTION__);

    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_endpoint.empty());

    // Initialize endpoint
    auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
    m_endpoint = GetRequestEndpoint(properties, RequestType::Synthesize).EndpointUrl();
    properties->Set(PropertyId::SpeechServiceConnection_Url, m_endpoint.c_str());
}

void CSpxRestTtsEngineAdapter::Term()
{
    // nothing to see here, move along
}

std::shared_ptr<ISpxSynthesisResult> CSpxRestTtsEngineAdapter::Speak(const std::string& text, bool isSsml, const std::string& requestId, bool retry)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_REST_TTS, __FUNCTION__);
    UNUSED(retry);

    GetSite()->SetAdapterFormat(this, CSpxSynthesisHelper::GetSpeechSynthesisOutputFormatFromString(m_requestFormatStr));
    auto properties = ISpxNamedProperties::shared_from_this();

    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE, properties == nullptr);
    auto subscriptionKey = properties->GetOr(PropertyId::SpeechServiceConnection_Key, "");
    auto token = properties->GetOr(PropertyId::SpeechServiceAuthorization_Token, "");

    auto ssml = text;
    std::shared_ptr<ISpxErrorInformation> ssmlError;
    if (!isSsml)
    {
        std::tie(ssml, ssmlError) = CSpxSynthesisHelper::BuildSsml(text, properties);
    }

    SPX_DBG_TRACE_VERBOSE("SSML sent to TTS cognitive service: %s", ssml.data());

    std::shared_ptr<ISpxSynthesisResult> result;

    InvokeOnSite([this, properties, requestId, ssml, subscriptionKey, token, ssmlError, &result](const SitePtr& p) {
        result = p->CreateEmptySynthesisResult();
        auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);

        if (ssmlError != nullptr)
        {
            resultInit->InitSynthesisResult(requestId, ResultReason::Canceled, ssmlError);
            return;
        }

        RestTtsRequest request;
        request.requestId = requestId;
        request.endpoint = m_endpoint;
        request.postContent = ssml;
        request.isSsml = true;
        request.subscriptionKey = subscriptionKey;
        request.accessToken = token;
        request.outputFormatString = this->m_requestFormatStr;
        request.adapter = this;
        request.site = p;
        request.userAgent = ConstructUserAgent();

        PostTtsRequest(request, resultInit, properties);
        if (result->GetReason() == ResultReason::Canceled)
        {
            if (GetSite()->AudioLengthOfCurrentTurn() == 0 && request.response.body.empty())
            {
                // Re-connect and re-send the request if disconnection happened and no data was received
                resultInit->Reset();
                SpxQueryInterface<ISpxNamedProperties>(resultInit)->Set(PropertyId::CancellationDetails_ReasonDetailedText, "");

                PostTtsRequest(request, resultInit, properties);
            }
        } else {
            InvokeOnSite([this](const SitePtr& p) { p->EndOfTurn(this); });
        }
    });

    return result;
}

std::shared_ptr<ISpxSynthesisResult> CSpxRestTtsEngineAdapter::Speak(std::shared_ptr<ISpxSynthesisRequestReader> request, bool retry)
{
    std::shared_ptr<ISpxSynthesisResult> result;
    switch (request->GetInputType())
    {
        case SynthesisRequestInputType::Text:
            return Speak(request->GetInputContent(), false, request->GetRequestId(), retry);
        case SynthesisRequestInputType::SSML:
            return Speak(request->GetInputContent(), true, request->GetRequestId(), retry);
        case SynthesisRequestInputType::TextStream:
            InvokeOnSite([request, &result](const SitePtr &p)
            {
                auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::BadRequest,
                                                          "Text steam is not supported by REST API, use WebSockets instead");
                result = p->CreateEmptySynthesisResult();
                auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);
                resultInit->InitSynthesisResult(request->GetRequestId(), ResultReason::Canceled, error);
            });
            return result;
        default:
            SPX_THROW_HR(SPXERR_INVALID_ARG);
    }
}

void CSpxRestTtsEngineAdapter::StopSpeaking(const std::shared_ptr<ISpxErrorInformation> &)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_REST_TTS, __FUNCTION__);
}

void CSpxRestTtsEngineAdapter::Connect()
{}

void CSpxRestTtsEngineAdapter::Disconnect(bool)
{}

std::unique_ptr<ISpxHttpResponse> CSpxRestTtsEngineAdapter::PostTtsRequest(RestTtsRequest& request, std::shared_ptr<ISpxSynthesisResultInit> result_init, ISpxNamedProperties::Ptr properties)
{
    auto endpoint = HttpEndpointInfo { request.endpoint };

    // Add http headers
    endpoint
        .SetHeader("User-Agent", request.userAgent)
        .SetHeader("X-Microsoft-OutputFormat", request.outputFormatString)
        .SetHeader("Content-Type", request.isSsml ? "application/ssml+xml" : "text/plain text")
        .SetHeader("X-ConnectionId", request.requestId);

    if (!request.subscriptionKey.empty())
    {
        endpoint.SetHeader("Ocp-Apim-Subscription-Key", request.subscriptionKey);
    }

    if (!request.accessToken.empty())
    {
        endpoint.SetHeader("Authorization", std::string("bearer ") + request.accessToken);
    }

    HttpUtils::ParseProxyConfig(properties.get(), endpoint);
    HttpUtils::ParseSSLConfig(properties.get(), endpoint);

    Maybe<std::string> additionalQueryParams = properties->Get(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters);
    if (additionalQueryParams)
    {
        for (const auto& kvp : HttpUtils::ParseQueryString(additionalQueryParams.Get()))
        {
            for (const auto& val : kvp.second)
            {
                endpoint.AddQueryParameter(kvp.first, val);
            }
        }
    }

    // Execute http request
    auto genericSite = request.site
        ? request.site->QueryInterface<ISpxGenericSite>()
        : std::shared_ptr<ISpxGenericSite> {};
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE, genericSite == nullptr);

    auto networkFactory = SpxQueryService<ISpxHttpTransportFactory>(genericSite);
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE, nullptr == networkFactory);

    auto rea_properties = genericSite->QueryInterface<ISpxNamedProperties>();
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE, nullptr == rea_properties);

    auto httpRequest = networkFactory->CreateHttpRequest(rea_properties, genericSite->QueryInterface<ISpxGenericSite>());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE, httpRequest == nullptr);

    try
    {
        auto httpResponse = httpRequest->SendRequestStreamResponse(
            HttpMethod::Post,
            endpoint,
            [requestPtr = &request](const uint8_t* data, const size_t size)
            {
                // NOTE: since the send doesn't return until all the data has been read, or we run into
                //       an error, we don't need to worry about the requestPtr going out of scope
                OnChunkReceived(requestPtr, data, size);
            },
            (const uint8_t*)request.postContent.c_str(),
            request.postContent.length());

        // Check the response
        if (httpResponse->IsSuccess())
        {
            // Write audio to result
            result_init->InitSynthesisResult(request.requestId, ResultReason::SynthesizingAudioCompleted, nullptr);
        }
        else
        {
            auto error = ErrorInfo::FromHttpStatus(
                static_cast<HttpStatusCode>(httpResponse->GetStatusCode()),
                httpResponse->GetReasonPhrase(),
                "TTS request failed:");
            result_init->InitSynthesisResult(request.requestId, ResultReason::Canceled, error);
        }

        return httpResponse;
    }
    catch (const HttpException& httpEx)
    {
        auto error = ErrorInfo::FromExplicitError(
            CancellationErrorCode::RuntimeError, // NOTE: This should probably be a ConnectionError
            std::string("Internal error: HttpAPI failed - ") + httpEx.what());
        result_init->InitSynthesisResult(request.requestId, ResultReason::Canceled, error);
    }
    catch (const std::exception& ex)
    {
        auto error = ErrorInfo::FromExplicitError(
            CancellationErrorCode::RuntimeError,
            std::string("Internal error: Unexpected exception while sending HTTP request - ") + ex.what());
        result_init->InitSynthesisResult(request.requestId, ResultReason::Canceled, error);
    }
    catch (...)
    {
        auto error = ErrorInfo::FromExplicitError(
            CancellationErrorCode::RuntimeError,
            "Internal error: Unexpected error while sending HTTP request");
        result_init->InitSynthesisResult(request.requestId, ResultReason::Canceled, error);
    }

    return std::unique_ptr<ISpxHttpResponse>(nullptr);
}

void CSpxRestTtsEngineAdapter::OnChunkReceived(RestTtsRequest* request, const uint8_t* buffer, size_t size)
{
    request->site->Write(request->adapter, request->requestId, const_cast<uint8_t *>(buffer), static_cast<uint32_t>(size), nullptr);

    // Append current chunk to total audio data for use of synthesis result
    std::unique_lock<std::mutex> lock(request->response.mutex);
    auto originalSize = request->response.body.size();
    request->response.body.resize(originalSize + size);
    memcpy(request->response.body.data() + originalSize, buffer, size);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
