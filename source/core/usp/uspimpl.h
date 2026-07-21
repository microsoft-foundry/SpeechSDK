//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <ajv.h>

#include "recognition_status.h"

#include "usp_endpoint.h"
#include "usp_enums.h"
#include "uspmessages.h"
#include "usp_web_socket.h"
#include "uspcommon.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

uint64_t telemetry_gettime();

constexpr auto HEADER_PATH = "Path";

constexpr auto azurecnRegion = "china";

constexpr std::array<const char*, 3> g_recoModeStrings{ { "interactive", "conversation", "dictation" } };

constexpr const char queryParameterDelim = '&';

enum class NetworkType
{
    Default,
    AzureCN
};

inline bool contains(const std::string& content, const std::string& name)
{
    return (content.find(name) != std::string::npos) ? true : false;
}

template<size_t N>
void BuildQueryParameters(const std::array<const char*, N>& parameterList, const std::map<std::string, std::string>& valueMap, bool isCustomEndpoint, std::ostringstream& oss)
{
    for (auto queryParameterName : parameterList)
    {
        if (!isCustomEndpoint || !contains(oss.str(), queryParameterName))
        {
            auto entry = valueMap.find(queryParameterName);
            if (entry != valueMap.end() && !entry->second.empty())
            {
                oss << queryParameterDelim << queryParameterName << entry->second;
            }
        }
    }
}

template <class T>
static void throw_if_null(const T* ptr, const std::string& name) {
    if (ptr == NULL)
    {
        ThrowInvalidArgumentException("The argument '" + name + "' is null."); \
    }
}

std::string TryGet(const UspHeaders& headers, const char* key);

NetworkType GetNetworkType(std::string region);

std::string GetHostNameSuffix(std::string region, EndpointType endpointType);

Impl::RecognitionStatus ToRecognitionStatus(const std::string& str);

KeywordVerificationStatus ToKeywordVerificationStatus(const std::string& str);

TranslationStatus ToTranslationStatus(const std::string& str);

ConfidenceLevel ToConfidenceLevel(const std::string& str);

template <class T = ajv::JsonReader>
std::string RetrievePrimaryLanguage(const T& json, std::string messagePath)
{
    std::string language;
    auto primaryLanguageJson = json[json_properties::primaryLanguage];
    if (primaryLanguageJson.IsObject())
    {
        language = primaryLanguageJson[json_properties::lang].AsString();
        if (language.empty())
        {
            PROTOCOL_VIOLATION("Invalid %s message, with primaryLanguage section but no language value. json = %s.", messagePath.c_str(), primaryLanguageJson.AsJson().c_str());
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("Got language %s from %s message.", language.c_str(), messagePath.c_str());
        }
    }

    return language;
}

SpeechHypothesisMsg RetrieveSpeechResult(const ajv::JsonReader& json);

TranslationResult RetrieveTranslationResult(const ajv::JsonReader& translationJson, bool expectStatus);

std::string GetHeadersAsString(const UspHeaders& headers);

}}}}
