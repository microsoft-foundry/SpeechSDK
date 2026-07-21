//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <sstream>
#include "http_utils.h"
#include "string_utils.h"
#include "speech_config.h"
#include "property_id_2_name_map.h"
#include "usp.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

void CSpxSpeechConfig::InitAuthorizationToken(const char* authToken, const char* region)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;

    CheckRegionString(region);
    Set(PropertyId::SpeechServiceAuthorization_Token, authToken);
    Set(PropertyId::SpeechServiceConnection_Region, region);
}

void CSpxSpeechConfig::InitFromEndpoint(const char* endpoint, const char* subscription)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;

    if (endpoint != nullptr)
    {
        string endpointString(endpoint);
        stringstream endpointStringStream(endpointString);
        vector<string> endpointTokens;
        string endpointToken;
        bool isV1Endpoint = false;
        while (getline(endpointStringStream, endpointToken, '/'))
        {
            if (!endpointToken.empty())
            {
                if (endpointToken.find("v1") == 0 || (endpointToken.find("V1") == 0) ||
                    endpointToken.find("api-version=1.") != string::npos)
                {
                    isV1Endpoint = true;
                }
            }
        }

        if (isV1Endpoint)
        {
            Set(g_isCustomV1Endpoint, true);
        }
        else if( (PAL::StringUtils::ToLower(endpointString).find(PAL::StringUtils::ToLower(USP::endpoint::unifiedspeech::unifiedPath)) != string::npos))
        {
            Set(g_isUnifiedSpeechEndpoint, true);
        }
    }

    Set(PropertyId::SpeechServiceConnection_Endpoint, endpoint);
    if (subscription != nullptr)
    {
        Set(PropertyId::SpeechServiceConnection_Key, subscription);
    }
}

void CSpxSpeechConfig::InitFromHost(const char* host, const char* subscription)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;

    Set(PropertyId::SpeechServiceConnection_Host, host);

    if (subscription != nullptr)
    {
        Set(PropertyId::SpeechServiceConnection_Key, subscription);
    }
}

void CSpxSpeechConfig::InitFromSubscription(const char* subscription, const char* region)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;

    CheckRegionString(region);
    Set(PropertyId::SpeechServiceConnection_Key, subscription);
    Set(PropertyId::SpeechServiceConnection_Region, region);
}

void CSpxSpeechConfig::InitEmbedded()
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;

    SetStringValue("CARBON-INTERNAL-UseEmbeddedSpeechConfig", "true");

    // Set recognition backend
    Set(PropertyId::SpeechServiceConnection_RecoBackend, "offline");

    // Set synthesis backend
    Set(PropertyId::SpeechServiceConnection_SynthBackend, "offline");
}

void CSpxSpeechConfig::SetServiceProperty(const string& name, const string& value, ServicePropertyChannel channel)
{
    std::string encodedName = HttpUtils::UrlEscape(name);
    std::string encodedValue = HttpUtils::UrlEscape(value);
    std::string encodedNameLowerCase = PAL::StringUtils::ToLower(encodedName);
    if (encodedNameLowerCase == "language" || encodedNameLowerCase == "format")
    {
        auto unsupportedV2ServiceProperties = GetOr(g_unsupportedV2ServiceProperties, "");
        unsupportedV2ServiceProperties += (unsupportedV2ServiceProperties.empty() ? "" : ", ") + encodedName;

        Set(g_unsupportedV2ServiceProperties, unsupportedV2ServiceProperties.c_str());
    }

    // parameters have been validated at C-API.
    if (channel == ServicePropertyChannel::UriQueryParameter)
    {
        auto currentParams = GetOr(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters, "");
        currentParams += (currentParams.empty() ? "" : "&") + encodedName + "=" + encodedValue;

        Set(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters, currentParams.c_str());
        std::string singleQueryParameterName = g_queryParameterPropertyNamePrefix + name;
        Set(singleQueryParameterName.c_str(), value);
    }
    else if (channel == ServicePropertyChannel::HttpHeader)
    {
        auto propertyName = string{ "HttpHeader" } + g_propertyNameSeparator + name;
        SetStringValue(propertyName.c_str(), value.c_str());
    }
    else
    {
        SPX_TRACE_ERROR("Unsupported channel: %d. Only UriQueryParameter is supported.", (int)channel);
        SPX_THROW_HR(SPXERR_INVALID_ARG);
    }
}

void CSpxSpeechConfig::SetProfanity(ProfanityOption profanity)
{
    string valueStr;
    switch (profanity)
    {
    case ProfanityOption::Masked:
        valueStr = USP::endpoint::profanityMasked;
        break;
    case ProfanityOption::Removed:
        valueStr = USP::endpoint::profanityRemoved;
        break;
    case ProfanityOption::Raw:
        valueStr = USP::endpoint::profanityRaw;
        break;
    default:
        SPX_TRACE_ERROR("Unsupported profanity: %d.", (int)profanity);
        SPX_THROW_HR(SPXERR_INVALID_ARG);
        break;
    }
    Set(PropertyId::SpeechServiceResponse_ProfanityOption, valueStr.c_str());
}

void CSpxSpeechConfig::CheckRegionString(const char *region)
{
    string regionStr(region);
    const auto forbiddenInRegion = {":", "//"};
    for(auto pattern: forbiddenInRegion)
    {
        if (regionStr.find(pattern) != string::npos)
        {
            SPX_TRACE_ERROR("Invalid region: %s.", region);
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
    }
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
