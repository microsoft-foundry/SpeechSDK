//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <assert.h>
#include <inttypes.h>
#include <sstream>

#include "uspimpl.h"

#include "uspinternal.h"

#include "usp_metrics.h"
#include "usp_message.h"

#include "exception.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include "string_utils.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

uint64_t telemetry_gettime()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return ms.count();
}

using namespace Microsoft::CognitiveServices::Speech::Impl;

/// <summary>
/// Helper method to read a value from constant the USP headers unordered map and return a default
/// empty string instead of an exception. Using [] would require a non-constant header, and .at
/// throws exceptions for missing keys
/// </summary>
/// <param name="headers">The headers to retrieve the value from</param>
/// <param name="key">The key to retrieve</param>
/// <returns>The value of that header, or an empty string if the key is not found</returns>
std::string TryGet(const UspHeaders& headers, const char * key)
{
    auto iter = headers.find(key);
    if (iter == headers.end())
    {
        return std::string{};
    }
    else
    {
        return iter->second;
    }
}

NetworkType GetNetworkType(std::string region)
{
    if (region.find(azurecnRegion) == 0)
    {
        return NetworkType::AzureCN;
    }
    else
    {
        return NetworkType::Default;
    }
}

std::string GetHostNameSuffix(std::string region, EndpointType endpointType)
{
    std::string result = "";
    NetworkType networkType = GetNetworkType(region);

    switch (endpointType)
    {
    case EndpointType::Speech:
        switch (networkType)
        {
        case NetworkType::AzureCN:
            result = region + endpoint::azurecnspeech::hostnameSuffix;
            break;
        default:
            result = region + endpoint::unifiedspeech::hostnameSuffix;
            break;
        }
        break;
    case EndpointType::StandaloneLanguageId:
        switch (networkType)
        {
        case NetworkType::AzureCN:
            result = region + endpoint::azurecnspeech::hostnameSuffix;
            break;
        default:
            result = region + endpoint::standalonelid::hostnameSuffix;
            break;
        }
        break;
    case EndpointType::Translation:
        switch (networkType)
        {
        case NetworkType::AzureCN:
            result = region + endpoint::azurecnspeech::hostnameSuffix;
            break;
        default:
            result = region + endpoint::unifiedspeech::hostnameSuffix;
            break;
        }
        break;
    case EndpointType::SpeechSynthesis:
        switch (networkType)
        {
        case NetworkType::AzureCN:
            result = region + endpoint::azurecnspeechsynthesis::hostnameSuffix;
            break;
        default:
            result = region + endpoint::speechSynthesis::hostnameSuffix;
            break;
        }
        break;
    case EndpointType::CustomVoice:
        switch (networkType)
        {
        case NetworkType::AzureCN:
            result = region + endpoint::azurecncustomvoice::hostnameSuffix;
            break;
        default:
            result = region + endpoint::customvoice::hostnameSuffix;
            break;
        }
        break;
    case EndpointType::Dialog:
        switch (networkType)
        {
        case NetworkType::AzureCN:
            result = region + endpoint::azurecndialog::hostnameSuffix;
            break;
        default:
            result = region + endpoint::dialog::hostnameSuffix;
            break;
        }
        break;
    default:
        break;
    }
    return result;
}


RecognitionStatus ToRecognitionStatus(const std::string& str)
{
    if (0 == str.compare("Success")) return  RecognitionStatus::Success;
    if (0 == str.compare("NoMatch")) return  RecognitionStatus::NoMatch;
    if (0 == str.compare("InitialSilenceTimeout")) return  RecognitionStatus::InitialSilenceTimeout;
    if (0 == str.compare("BabbleTimeout")) return RecognitionStatus::InitialBabbleTimeout;
    if (0 == str.compare("Error")) return RecognitionStatus::Error;
    if (0 == str.compare("EndOfDictation")) return RecognitionStatus::EndOfDictation;
    if (0 == str.compare("TooManyRequests")) return RecognitionStatus::TooManyRequests;
    if (0 == str.compare("BadRequest")) return RecognitionStatus::BadRequest;
    if (0 == str.compare("Forbidden")) return RecognitionStatus::Forbidden;
    if (0 == str.compare("ServiceUnavailable")) return RecognitionStatus::ServiceUnavailable;

    PROTOCOL_VIOLATION("Unknown RecognitionStatus: %s", str.c_str());
    return RecognitionStatus::InvalidMessage;
}

// Function to convert keyword verification status string to KeywordVerificationStatus enum.
KeywordVerificationStatus ToKeywordVerificationStatus(const std::string& str)
{
    if (0 == str.compare("Accepted")) return KeywordVerificationStatus::Accepted;
    if (0 == str.compare("Rejected")) return KeywordVerificationStatus::Rejected;

    PROTOCOL_VIOLATION("Unknown KeywordVerificationStatus: %s", str.c_str());
    return KeywordVerificationStatus::InvalidMessage;
}

TranslationStatus ToTranslationStatus(const std::string& str)
{
    if (0 == str.compare("Success")) return  TranslationStatus::Success;
    if (0 == str.compare("Error")) return  TranslationStatus::Error;

    PROTOCOL_VIOLATION("Unknown TranslationStatus: %s", str.c_str());
    return TranslationStatus::InvalidMessage;
}

ConfidenceLevel ToConfidenceLevel(const std::string& str)
{
    if (0 == str.compare("Unknown")) return  ConfidenceLevel::Unknown;
    if (0 == str.compare("Low")) return  ConfidenceLevel::Low;
    if (0 == str.compare("Medium")) return  ConfidenceLevel::Medium;
    if (0 == str.compare("High")) return  ConfidenceLevel::High;
    PROTOCOL_VIOLATION("Invalid ConfidenceLevel: %s", str.c_str());
    return ConfidenceLevel::InvalidMessage;
}

SpeechHypothesisMsg RetrieveSpeechResult(const ajv::JsonReader& json)
{
    auto offset = json[json_properties::offset].AsUint<uint64_t>();
    auto duration = json[json_properties::duration].AsUint<uint64_t>();
    auto text = json[json_properties::text].AsString();
    auto language = RetrievePrimaryLanguage(json, "speech.hypothesis");
    return SpeechHypothesisMsg(json.AsJson(), offset, duration, std::move(text), "", "", std::move(language));
}

TranslationResult RetrieveTranslationResult(const ajv::JsonReader& translationJson, bool expectStatus)
{
    TranslationResult result;
    if (expectStatus)
    {
        auto status = translationJson[json_properties::translationStatus];
        if (status.IsString())
        {
            result.translationStatus = ToTranslationStatus(status.AsString());
        }
        else
        {
            PROTOCOL_VIOLATION("No TranslationStatus is provided. Json: %s", translationJson.AsJson().c_str());
            result.translationStatus = TranslationStatus::InvalidMessage;
            result.failureReason = L"Status is missing in the protocol message. Response text:" + PAL::ToWString(translationJson.AsJson());
        }

        auto failure = translationJson[json_properties::failureReason];
        if (failure.IsString())
        {
            result.failureReason += PAL::ToWString(failure.AsString());
        }
    }

    if (expectStatus && result.translationStatus != TranslationStatus::Success)
    {
        return result;
    }
    else
    {
        auto translations = translationJson[json_properties::translations];
        for (auto object = translations.FirstValue(); object.IsOk(); ++object)
        {
            auto lang = object[json_properties::lang].AsString();
            auto txt = object[json_properties::text].AsString();
            if (txt.empty())
            {
                txt = object[json_properties::displayText].AsString();
            }

            if (lang.empty() && txt.empty())
            {
                PROTOCOL_VIOLATION("empty language and text field in translations text. lang=%s, text=%s. json=%s", lang.c_str(), txt.c_str(), object.AsJson().c_str());
                continue;
            }
            result.translations.push_back(std::make_tuple(std::move(lang), std::move(txt)));
        }

        if (!result.translations.size())
        {
            PROTOCOL_VIOLATION("No Translations text block in the message. Response text: %s", translationJson.AsJson().c_str());
        }
        return result;
    }
}

std::string GetHeadersAsString(const UspHeaders& headers)
{
    std::ostringstream oss;
    for (const auto& entry : headers)
    {
        // NOTE: for consistency with the previous implementation, I left the extra new line at the beginning
        //       of the headers string
        oss << "\r\n";
        oss << entry.first << ": " << entry.second;
    }

    return oss.str();
}

}}}}
