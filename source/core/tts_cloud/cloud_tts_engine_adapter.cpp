//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// cloud_tts_engine_adapter.cpp: Implementation definitions for CSpxCloudTtsEngineAdapter C++ class
//

#include "stdafx.h"
#include "synthesis_helper.h"
#include "guid_utils.h"
#include "handle_table.h"
#include "service_helpers.h"
#include "property_bag_impl.h"
#include "property_id_2_name_map.h"
#include "spx_build_information.h"
#include "time_utils.h"
#include "thread_service.h"
#include "error_info.h"
#include "cloud_tts_engine_adapter.h"
#include "usp.h"
#include "endpoint_utils.h"
#include "interfaces/ispx_http_request.h"
#include "interfaces/ispx_http_transport_factory.h"
#include "http_headers.h"
#include "http_status_codes.h"
#include "codec_helpers.h"
#include "audio_format_id_2_name_map.h"
#include "create_object_helpers.h"
#include "http_exception.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

std::shared_ptr<ISpxSynthesisVoicesResult> CSpxCloudTtsEngineAdapter::GetVoices(const std::string& locale)
{
    SPX_DBG_TRACE_VERBOSE(__FUNCTION__);

    auto ttsEngineSite = GetSite();
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, ttsEngineSite == nullptr);

    auto result = ttsEngineSite->CreateEmptySynthesisVoicesResult();
    auto resultInit = SpxQueryInterface<ISpxSynthesisVoicesResultInit>(result);

    auto requestId = PAL::ToString(PAL::CreateGuidWithoutDashes());

    auto properties = SpxQueryService<ISpxNamedProperties>(ttsEngineSite);
    auto subscriptionKey = properties->GetOr(PropertyId::SpeechServiceConnection_Key, "");
    auto token = properties->GetOr(PropertyId::SpeechServiceAuthorization_Token, "");
    auto endpoint = GetRequestEndpoint(properties, RequestType::VoicesList);

    HttpUtils::ParseProxyConfig(properties.get(), endpoint, false);
    HttpUtils::ParseSSLConfig(properties.get(), endpoint);

    endpoint.SetHeader(USP::headers::ocpApimSubscriptionKey, subscriptionKey);
    endpoint.SetHeader(USP::headers::authorization, "Bearer " + token);
    endpoint.SetHeader(USP::headers::userAgent, ConstructUserAgent());
    endpoint.SetHeader(USP::headers::connectionId, requestId);

    auto networkFactory = SpxQueryService<ISpxHttpTransportFactory>(ttsEngineSite);
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE, networkFactory == nullptr);

    auto rea_properties = SpxQueryService<ISpxNamedProperties>(ISpxInterfaceBase::shared_from_this());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, nullptr == properties);

    auto request = networkFactory->CreateHttpRequest(rea_properties, ttsEngineSite->QueryInterface<ISpxGenericSite>());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE, request == nullptr);


    std::unique_ptr<ISpxHttpResponse> response;
    try
    {
        response = request->SendRequest(HttpMethod::Get, endpoint);
    }
    catch (HttpException& e)
    {
        std::string message{ "Error in sending a http request. Details: " };
        message += e.what();
        SPX_TRACE_ERROR("%s", message.c_str());
        resultInit->InitErrorResult(ErrorInfo::FromExplicitError(CancellationErrorCode::ConnectionFailure, message), requestId);
        return result;
    }
    catch (...)
    {
        SPX_TRACE_ERROR("Unknown error in sending http request");
    }

    if (response == nullptr)
    {
        return nullptr;
    }

    if (!(response->IsSuccess()))
    {
        SPX_TRACE_ERROR("SendRequest failed with HTTP %u [%s] url:'%s'",
            response->GetStatusCode(),
            response->ReadContentAsString().c_str(),
            endpoint.EndpointUrl().c_str());

        auto error = ErrorInfo::FromHttpStatus(static_cast<HttpStatusCode>(response->GetStatusCode()),
                                               "", "Get voices list failed:", "");
        resultInit->InitErrorResult(error, requestId);
    }
    else
    {
        auto rawVoices = ajv::json::Parse(response->ReadContentAsString());
        auto initialized = false;

        auto localeEmpty = locale.empty();
        auto lowerLocale = PAL::StringUtils::ToLower(locale);
        for (auto item = rawVoices[0]; !item.IsEnd(); ++item)
        {
            if (localeEmpty || (item.ValueAt(VoiceJson::locale).IsOk() && PAL::StringUtils::ToLower(item[VoiceJson::locale].AsString()) == lowerLocale))
            {
                if (!initialized)
                {
                    resultInit->InitSuccessResult(requestId);
                    initialized = true;
                }

                resultInit->AppendVoice(CreateVoiceInfo(item));
            }
        }

        if (!initialized)
        {
            auto errorMessage = "No voice matches locale [" + locale + "].";
            resultInit->InitErrorResult(ErrorInfo::FromRuntimeMessage(errorMessage), requestId);
        }

        auto resultProperties = SpxQueryInterface<ISpxNamedProperties>(result);
        resultProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, response->ReadContentAsString().c_str());
    }

    return result;
}

void CSpxCloudTtsEngineAdapter::SetOutput(const std::shared_ptr<ISpxAudioOutput>& output)
{
    SPX_DBG_TRACE_VERBOSE( __FUNCTION__);
    m_audioOutput = output;

    auto outputFormat = SpxQueryInterface<ISpxAudioOutputFormat>(output);
    auto outputFormatStr = outputFormat->GetFormatString();
    auto hasHeader = outputFormat->HasHeader();

    auto format = CSpxSynthesisHelper::GetSpeechSynthesisOutputFormatFromString(outputFormatStr);

    auto maybeCompressedProp = Get<bool>(PropertyId::SpeechServiceConnection_SynthEnableCompressedAudioTransmission);
    const auto useCompressedAudioOnWire = maybeCompressedProp ?
        maybeCompressedProp.Get()
        : UsingCodecByDefault() && IsCodecAdapterAvailable();

    if (auto maybeTransmitFormatProp = Get("SPEECH-SynthTransmissionFormat"))
    {
        if (maybeTransmitFormatProp.Get() == outputFormatStr)
        {
            if (hasHeader)
            {
                ThrowInvalidArgumentException("You shouldn't use a riff format for transmission as the streaming is not supported.");
            }
        }

        auto transmitFormat = CSpxSynthesisHelper::GetSpeechSynthesisOutputFormatFromString(
            maybeTransmitFormatProp.Get());

        std::vector<int16_t> supportedDecodingCodecs = { WAVE_FORMAT_MP3,
#if __ANDROID__
                                                         WAVE_FORMAT_OPUS
#elif ___APPLE__
#else
                                                         WAVE_FORMAT_OGG_OPUS,
                                                         WAVE_FORMAT_WEBM_OPUS
#endif
        };
        if (std::find(supportedDecodingCodecs.begin(), supportedDecodingCodecs.end(), transmitFormat->wFormatTag) == supportedDecodingCodecs.end())
        {
            ThrowInvalidArgumentException("The requested format is not supported for decoding.");
        }
        else if (format->wFormatTag != WAVE_FORMAT_PCM)
        {
            ThrowInvalidArgumentException("Speech synthesis output format should be PCM when transmission format is specified.");
        }

        m_requestFormatStr = maybeTransmitFormatProp.Get();
    }
    // Don't use compressed audio for sample rate <= 8kHz.
    else if (useCompressedAudioOnWire && format->wFormatTag == WAVE_FORMAT_PCM && format->nSamplesPerSec > 8000
#if __ANDROID__
        && format->nSamplesPerSec % 8000 == 0
#endif
    )
    {
        SPX_TRACE_INFO("%s: UseCompressedAudioOnWire is set to true, will use compressed format for transmission.", __FUNCTION__);
        // We use 24kHz 96 bitrate mp3 format for default. Opus codec has better quality, but the oggdemux needs to wait whole ogg page for validating.
        // This will buffer ~4KB (~500ms) audio. Ideally, we'd better to use opus streaming or opus with WebM container.
        switch (format->nSamplesPerSec)
        {
        case 16000:
            m_requestFormatStr = GetAudioFormatName(SpeechSynthesisOutputFormat::Audio16Khz32KBitRateMonoMp3);
            break;
        case 24000:
        case 22050:
            m_requestFormatStr = GetAudioFormatName(SpeechSynthesisOutputFormat::Audio24Khz48KBitRateMonoMp3);
            break;
        case 48000:
        case 44100:
            m_requestFormatStr = GetAudioFormatName(SpeechSynthesisOutputFormat::Audio48Khz96KBitRateMonoMp3);
            break;
        default:
            SPX_TRACE_WARNING("%s: Unsupported sample rate %d, don't use compressed format.", __FUNCTION__, format->nSamplesPerSec);
            m_requestFormatStr = outputFormatStr;
            break;
        }
    }
    else if (hasHeader)
    {
        SPX_TRACE_INFO("%s: request format [%s] has header, using raw format instead for streaming.", __FUNCTION__, outputFormatStr.c_str());
        m_requestFormatStr = outputFormatStr;
        std::string replaceTag;
        if (format->wFormatTag == WAVE_FORMAT_PCM || format->wFormatTag == WAVE_FORMAT_MULAW || format->wFormatTag == WAVE_FORMAT_ALAW)
        {
            replaceTag = "raw";
        }
        else if (format->wFormatTag == WAVE_FORMAT_SIREN)
        {
            replaceTag = "audio";
        }

        m_requestFormatStr.replace(0, 4, replaceTag);
    }
    else
    {
        m_requestFormatStr = outputFormatStr;
    }
}

std::shared_ptr<ISpxNamedProperties> CSpxCloudTtsEngineAdapter::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

HttpEndpointInfo CSpxCloudTtsEngineAdapter::GetRequestEndpoint(std::shared_ptr<ISpxNamedProperties> properties, RequestType requestType)
{
    auto endpointUrl = properties->GetOr(PropertyId::SpeechServiceConnection_Endpoint, "");
    auto hostUrl = properties->GetOr(PropertyId::SpeechServiceConnection_Host, "");
    auto region = properties->GetOr(PropertyId::SpeechServiceConnection_Region, "");
    const auto endpointId = properties->GetOr(PropertyId::SpeechServiceConnection_EndpointId, "");

    if (requestType == RequestType::VoicesList)
    {
        auto voiceListEndpoint = properties->GetOr(PropertyId::SpeechServiceConnection_VoicesListEndpoint, "");
        if (!voiceListEndpoint.empty())
        {
            return HttpEndpointInfo(voiceListEndpoint);
        }
    }

    HttpEndpointInfo endpoint;
    bool endpointConstructed = false;
    bool customDomainSynthEndpoint = false;

    string pathPrefix = "";

    // Use custom endpoint if available and valid
    if (!endpointUrl.empty())
    {
        std::string endpointRegion;
        if (requestType == RequestType::Synthesize)
        {
            // Check for invalid use of auth token service endpoint

            bool isTokenServiceEndpoint = false;
            std::tie(isTokenServiceEndpoint, endpointRegion) =
                EndpointUtils::IsTokenServiceEndpoint(endpointUrl);

            if (isTokenServiceEndpoint)
            {
                // Ignore the custom endpoint and use defaults except for region
                if (!endpointRegion.empty())
                {
                    region = endpointRegion;
                }
            }
            else
            {
                // A custom endpoint that only specifies a host (no path/query) is treated
                // as a custom-domain endpoint: build the REST synthesis path from it the same
                // way the USP (WebSocket) adapter does. Endpoints that already carry an explicit
                // path or query string are used verbatim.
                auto url = HttpUtils::ParseUrl(endpointUrl);
                if (url.path.empty() && url.query.empty())
                {
                    endpoint.Scheme(UriScheme::HTTPS).Host(url.host);
                    if (url.port > 0)
                    {
                        endpoint.Port(url.port);
                    }
                    customDomainSynthEndpoint = true;
                }
                else
                {
                    endpoint.EndpointUrl(endpointUrl);
                    endpointConstructed = true;
                }
            }
        }
        else if (requestType == RequestType::VoicesList)
        {
            // parse endpoint url to get the host.
            auto url = HttpUtils::ParseUrl(endpointUrl);

            // if url path starts with "/tts" or "/voice", copy this prefix to the endpoint
            if (url.path.find(USP::endpoint::speechSynthesis::customDomainPrefix) == 0)
            {
                pathPrefix = USP::endpoint::speechSynthesis::customDomainPrefix;
            }
            else if (url.path.find(USP::endpoint::customvoice::customDomainPrefix) == 0)
            {
                pathPrefix = USP::endpoint::customvoice::customDomainPrefix;
            }

            if (hostUrl.empty())
            {
                endpoint.Scheme(UriScheme::HTTPS)
                    .Host(url.host);
            }
        }
    }

    // Use custom host if available and there was no custom endpoint
    if (!endpointConstructed)
    {
        if (!hostUrl.empty())
        {
            endpoint.EndpointUrl(hostUrl);

            // Check FromHost restrictions
            if (!(endpoint.Path().empty() || endpoint.Path() == "/"))
            {
                ThrowInvalidArgumentException("Resource path is not allowed in the host URI.");
            }
            if (!endpoint.QueryString().empty())
            {
                ThrowInvalidArgumentException("Query parameters are not allowed in the host URI.");
            }

        }
        else if(!region.empty() && !customDomainSynthEndpoint)
        {
            endpoint.Scheme(UriScheme::HTTPS);

            if (endpointId.empty())
            {
                endpoint.Host(region + USP::endpoint::speechSynthesis::hostnameSuffix);
            }
            else
            {
                endpoint.Host(region + USP::endpoint::customvoice::hostnameSuffix);
            }
        }

        if (requestType == RequestType::Synthesize)
        {
            endpoint.Path(customDomainSynthEndpoint
                ? USP::endpoint::speechSynthesis::restCustomDomainPath
                : USP::endpoint::speechSynthesis::restPath);
        }
        else if (requestType == RequestType::VoicesList)
        {
            if (!endpointUrl.empty()) // If the endpoint was specified we're building the path and always want to connect to /tts/
            {
                endpoint.Path(pathPrefix + (pathPrefix.empty() ? USP::endpoint::speechSynthesis::voicesListPath : USP::endpoint::speechSynthesis::voicesHostListPath));
            }
            else
            {
                endpoint.Path(pathPrefix + USP::endpoint::speechSynthesis::voicesHostListPath);
            }
        }

        if (!endpointId.empty() && requestType == RequestType::Synthesize)
        {
            endpoint.AddQueryParameter("deploymentId", endpointId);
        }
    }

    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !endpoint.IsValid());
    return endpoint;
}

std::string CSpxCloudTtsEngineAdapter::ConstructUserAgent()
{
    auto existingUserAgent = GetOr("HttpHeader#User-agent", "");
    if (!existingUserAgent.empty())
    {
        return existingUserAgent;
    }

    auto lang = GetOr("AZAC-SDK-PROGRAMMING-LANGUAGE", BuildInformation::g_programmingLanguage);
    auto platform = PAL::getOperatingSystem().to_string();

    auto appOverride = GetOr("AZAC-SDK-APPNAME", "");
    const char* app = nullptr;
    if (!appOverride.empty())
    {
        app = appOverride.c_str();
    }
    else
    {
        auto queryParams = GetOr(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters, "");
        app = queryParams.find("traffictype=spx") != std::string::npos ? "spx" : nullptr;
    }

    auto agent = HttpUtils::FormatAzSdkUserAgent(app, BuildInformation::g_SpeechSDKName, lang.c_str(), BuildInformation::g_fullSpeechVersion, platform.c_str());
    return agent;
}

std::shared_ptr<ISpxVoiceInfo> CSpxCloudTtsEngineAdapter::CreateVoiceInfo(const ajv::JsonReader& obj) const
{
    auto voice = GetSite()->CreateEmptyVoiceInfo();
    auto voiceInit = SpxQueryInterface<ISpxVoiceInfoInit>(voice);
    auto voiceProperties = SpxQueryInterface<ISpxNamedProperties>(voice);

    auto name = obj.FirstName();
    while (name.IsOk())
    {
        auto key = name++.AsString();
        auto item = obj[key.c_str()];
        auto value = item.IsString() ? item.AsString() : item.AsJson();
        voiceProperties->SetStringValue(key.c_str(), value.c_str());
    }

    const auto voiceType = obj[VoiceJson::voiceType].AsString() == "Neural"
                               ? SynthesisVoiceType::OnlineNeural
                               : SynthesisVoiceType::OnlineStandard;
    voiceInit->InitVoiceInfo(obj[VoiceJson::name].AsString(), obj[VoiceJson::locale].AsString(), voiceType);
    voiceInit->SetNames(obj[VoiceJson::shortName].AsString(), obj[VoiceJson::localName].AsString());

    std::vector<string> styleList;
    for (auto it = obj[VoiceJson::styleList][0]; it.IsOk(); it++) {
        styleList.emplace_back(it.AsString());
    }
    voiceInit->SetStyleList(std::move(styleList));

    return voice;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
