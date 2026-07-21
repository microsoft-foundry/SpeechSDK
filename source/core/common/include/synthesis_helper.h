//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <array>
#include <sstream>

#include "string_utils.h"
#include "property_id_2_name_map.h"
#include "error_info.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

constexpr auto TTS_COGNITIVE_SERVICE_HOST_SUFFIX = ".tts.speech.microsoft.com";
constexpr auto TTS_COGNITIVE_SERVICE_URL_PATH = "/cognitiveservices/v1";

constexpr uint32_t RIFF_MARKER = 0x46464952;
constexpr uint32_t WAVE_MARKER = 0x45564157;
constexpr uint32_t FMT_MARKER = 0x20746d66;
constexpr uint32_t DATA_MARKER = 0x61746164;
constexpr uint32_t EVNT_MARKER = 0x544e5645;

template<typename T>
void buffer_write(uint8_t** buffer_cursor, T value)
{
    auto buf = *buffer_cursor;
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        *buf = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
        ++buf;
    }
    *buffer_cursor = buf;
}

struct RIFFHDR
{
    uint32_t _id;
    uint32_t _len;              /* file length less header */
    uint32_t _type;             /* should be "WAVE" */

    RIFFHDR(uint32_t length)
    {
        _id = RIFF_MARKER;
        _type = WAVE_MARKER;
        _len = length;
    }
};

struct BLOCKHDR
{
    uint32_t _id;              /* should be "fmt " or "data" */
    uint32_t _len;             /* block size less header */

    BLOCKHDR(uint32_t length)
    {
        _id = FMT_MARKER;
        _len = length;
    }
};

struct DATAHDR
{
    uint32_t _id;               /* should be "fmt " or "data" */
    uint32_t _len;              /* block size less header */

    DATAHDR(uint32_t length)
    {
        _id = DATA_MARKER;
        _len = length;
    }
};

struct EVNTHDR
{
    uint32_t _id;               /* should be "EVNT" */
    uint32_t _len;              /* block size less header */

    EVNTHDR(uint32_t length)
    {
        _id = EVNT_MARKER;
        _len = length;
    }
};

struct SynthesisAudioFormat
{
public:
    SpxWAVEFORMATEX_Type outputFormat; // Output WAVE format (return to customer)
    SpxWAVEFORMATEX_Type requestFormat; // Request WAVE format (transmission on wire)
    std::string outputFormatString; // output format string
    bool hasHeader = false; // if the output format has header
    std::string requestFormatString; // format string sent to text--to-speech service
};

class CSpxSynthesisHelper
{
public:

    static std::pair<std::string, std::string> GetLanguageAndVoice(const std::shared_ptr<ISpxNamedProperties>& properties)
    {
        using tuple_type = std::tuple<const char *, const char *>;
        constexpr std::array<tuple_type, 148> languageToDefaultVoice{ {
            tuple_type{ "af-za", "Adri" },
            tuple_type{ "am-et", "Mekdes" },
            tuple_type{ "ar-ae", "Fatima" },
            tuple_type{ "ar-bh", "Laila" },
            tuple_type{ "ar-dz", "Amina" },
            tuple_type{ "ar-eg", "Salma" },
            tuple_type{ "ar-iq", "Rana" },
            tuple_type{ "ar-jo", "Sana" },
            tuple_type{ "ar-kw", "Noura" },
            tuple_type{ "ar-lb", "Layla" },
            tuple_type{ "ar-ly", "Iman" },
            tuple_type{ "ar-ma", "Mouna" },
            tuple_type{ "ar-om", "Aysha" },
            tuple_type{ "ar-qa", "Amal" },
            tuple_type{ "ar-sa", "Zariyah" },
            tuple_type{ "ar-sy", "Amany" },
            tuple_type{ "ar-tn", "Reem" },
            tuple_type{ "ar-ye", "Maryam" },
            tuple_type{ "az-az", "Banu" },
            tuple_type{ "bg-bg", "Kalina" },
            tuple_type{ "bn-bd", "Nabanita" },
            tuple_type{ "bn-in", "Tanishaa" },
            tuple_type{ "bs-ba", "Vesna" },
            tuple_type{ "ca-es", "Joana" },
            tuple_type{ "cs-cz", "Vlasta" },
            tuple_type{ "cy-gb", "Nia" },
            tuple_type{ "da-dk", "Christel" },
            tuple_type{ "de-at", "Ingrid" },
            tuple_type{ "de-ch", "Leni" },
            tuple_type{ "de-de", "Katja" },
            tuple_type{ "el-gr", "Athina" },
            tuple_type{ "en-au", "Natasha" },
            tuple_type{ "en-ca", "Clara" },
            tuple_type{ "en-gb", "Sonia" },
            tuple_type{ "en-hk", "Yan" },
            tuple_type{ "en-ie", "Emily" },
            tuple_type{ "en-in", "Neerja" },
            tuple_type{ "en-ke", "Asilia" },
            tuple_type{ "en-ng", "Ezinne" },
            tuple_type{ "en-nz", "Molly" },
            tuple_type{ "en-ph", "Rosa" },
            tuple_type{ "en-sg", "Luna" },
            tuple_type{ "en-tz", "Imani" },
            tuple_type{ "en-us", "AvaMultilingual" },
            tuple_type{ "en-za", "Leah" },
            tuple_type{ "es-ar", "Elena" },
            tuple_type{ "es-bo", "Sofia" },
            tuple_type{ "es-cl", "Catalina" },
            tuple_type{ "es-co", "Salome" },
            tuple_type{ "es-cr", "Maria" },
            tuple_type{ "es-cu", "Belkys" },
            tuple_type{ "es-do", "Ramona" },
            tuple_type{ "es-ec", "Andrea" },
            tuple_type{ "es-es", "Elvira" },
            tuple_type{ "es-gq", "Teresa" },
            tuple_type{ "es-gt", "Marta" },
            tuple_type{ "es-hn", "Karla" },
            tuple_type{ "es-mx", "Dalia" },
            tuple_type{ "es-ni", "Yolanda" },
            tuple_type{ "es-pa", "Margarita" },
            tuple_type{ "es-pe", "Camila" },
            tuple_type{ "es-pr", "Karina" },
            tuple_type{ "es-py", "Tania" },
            tuple_type{ "es-sv", "Lorena" },
            tuple_type{ "es-us", "Paloma" },
            tuple_type{ "es-uy", "Valentina" },
            tuple_type{ "es-ve", "Paola" },
            tuple_type{ "et-ee", "Anu" },
            tuple_type{ "eu-es", "Ainhoa" },
            tuple_type{ "fa-ir", "Dilara" },
            tuple_type{ "fi-fi", "Selma" },
            tuple_type{ "fil-ph", "Blessica" },
            tuple_type{ "fr-be", "Charline" },
            tuple_type{ "fr-ca", "Sylvie" },
            tuple_type{ "fr-ch", "Ariane" },
            tuple_type{ "fr-fr", "Denise" },
            tuple_type{ "ga-ie", "Orla" },
            tuple_type{ "gl-es", "Sabela" },
            tuple_type{ "gu-in", "Dhwani" },
            tuple_type{ "he-il", "Hila" },
            tuple_type{ "hi-in", "Swara" },
            tuple_type{ "hr-hr", "Gabrijela" },
            tuple_type{ "hu-hu", "Noemi" },
            tuple_type{ "hy-am", "Anahit" },
            tuple_type{ "id-id", "Gadis" },
            tuple_type{ "is-is", "Gudrun" },
            tuple_type{ "it-it", "Elsa" },
            tuple_type{ "ja-jp", "Nanami" },
            tuple_type{ "jv-id", "Siti" },
            tuple_type{ "ka-ge", "Eka" },
            tuple_type{ "kk-kz", "Aigul" },
            tuple_type{ "km-kh", "Sreymom" },
            tuple_type{ "kn-in", "Sapna" },
            tuple_type{ "ko-kr", "SunHi" },
            tuple_type{ "lo-la", "Keomany" },
            tuple_type{ "lt-lt", "Ona" },
            tuple_type{ "lv-lv", "Everita" },
            tuple_type{ "mk-mk", "Marija" },
            tuple_type{ "ml-in", "Sobhana" },
            tuple_type{ "mn-mn", "Yesui" },
            tuple_type{ "mr-in", "Aarohi" },
            tuple_type{ "ms-my", "Yasmin" },
            tuple_type{ "mt-mt", "Grace" },
            tuple_type{ "my-mm", "Nilar" },
            tuple_type{ "nb-no", "Pernille" },
            tuple_type{ "ne-np", "Hemkala" },
            tuple_type{ "nl-be", "Dena" },
            tuple_type{ "nl-nl", "Fenna" },
            tuple_type{ "pl-pl", "Agnieszka" },
            tuple_type{ "ps-af", "Latifa" },
            tuple_type{ "pt-br", "Francisca" },
            tuple_type{ "pt-pt", "Raquel" },
            tuple_type{ "ro-ro", "Alina" },
            tuple_type{ "ru-ru", "Svetlana" },
            tuple_type{ "si-lk", "Thilini" },
            tuple_type{ "sk-sk", "Viktoria" },
            tuple_type{ "sl-si", "Petra" },
            tuple_type{ "so-so", "Ubax" },
            tuple_type{ "sq-al", "Anila" },
            tuple_type{ "sr-latn-rs", "Nicholas" },
            tuple_type{ "sr-rs", "Sophie" },
            tuple_type{ "su-id", "Tuti" },
            tuple_type{ "sv-se", "Sofie" },
            tuple_type{ "sw-ke", "Zuri" },
            tuple_type{ "sw-tz", "Rehema" },
            tuple_type{ "ta-in", "Pallavi" },
            tuple_type{ "ta-lk", "Saranya" },
            tuple_type{ "ta-my", "Kani" },
            tuple_type{ "ta-sg", "Venba" },
            tuple_type{ "te-in", "Shruti" },
            tuple_type{ "th-th", "Premwadee" },
            tuple_type{ "tr-tr", "Emel" },
            tuple_type{ "uk-ua", "Polina" },
            tuple_type{ "ur-in", "Gul" },
            tuple_type{ "ur-pk", "Uzma" },
            tuple_type{ "uz-uz", "Madina" },
            tuple_type{ "vi-vn", "HoaiMy" },
            tuple_type{ "wuu-cn", "Xiaotong" },
            tuple_type{ "yue-cn", "XiaoMin" },
            tuple_type{ "zh-cn", "Xiaoxiao" },
            tuple_type{ "zh-cn-henan", "Yundeng" },
            tuple_type{ "zh-cn-liaoning", "Xiaobei" },
            tuple_type{ "zh-cn-shaanxi", "Xiaoni" },
            tuple_type{ "zh-cn-shandong", "Yunxiang" },
            tuple_type{ "zh-cn-sichuan", "Yunxi" },
            tuple_type{ "zh-hk", "HiuMaan" },
            tuple_type{ "zh-tw", "HsiaoChen" },
            tuple_type{ "zu-za", "Thando" }
        } };

        std::string chosenLanguage = properties->GetOr(PropertyId::SpeechServiceConnection_SynthLanguage, "");
        std::string chosenVoice = properties->GetOr(PropertyId::SpeechServiceConnection_SynthVoice, "");

        if (LanguageAutoDetectionEnabled(properties))
        {
            // Set default language to en-US
            chosenLanguage = "en-US";
        }
        else if (chosenVoice.empty())
        {
            if (chosenLanguage.empty())
            {
                SPX_TRACE_INFO("Neither language nor voice is specified, use Ava multilingual voice with language auto detection.");
                chosenVoice = "en-US-AvaMultilingualNeural";
            }
            else
            {
                // If language is not empty, convert it to lower case (e.g. zh-CN -> zh-cn
                auto lowerLanguage = PAL::StringUtils::ToLower(chosenLanguage);
                chosenVoice.reserve(30);

                // Set default voice based on language
                auto it = std::find_if(languageToDefaultVoice.begin(), languageToDefaultVoice.end(), [&lowerLanguage](const tuple_type& item)
                {
                    const auto lang = std::get<0>(item);
                    return PAL::stricmp(lang, lowerLanguage.c_str()) == 0;
                });
                if (it != languageToDefaultVoice.end())
                {
                    chosenVoice.append(lowerLanguage);
                    chosenVoice.append("-");
                    chosenVoice.append(std::get<1>(*it));
                    chosenVoice.append("Neural");
                }
                else
                {
                    // If it's not found, return empty voice to indicate the language is not supported.
                    SPX_TRACE_ERROR("Language %s is not supported.", chosenLanguage.c_str());
                    return std::make_pair(chosenLanguage, "");
                }
            }
        }

        return std::make_pair(chosenLanguage, chosenVoice);
    }

    static std::pair<std::string, std::shared_ptr<ISpxErrorInformation>> BuildSsml(const std::string& text, const std::shared_ptr<ISpxNamedProperties>& properties)
    {
        std::string chosenLanguage, chosenVoice;
        std::tie(chosenLanguage, chosenVoice) = GetLanguageAndVoice(properties);
        if (chosenVoice.empty() && !LanguageAutoDetectionEnabled(properties))
        {
            std::stringstream errorMessage;
            errorMessage << "Language '" << chosenLanguage << "' is not supported "
                    << "by the SDK. Please set the voice explicitly or use SSML.";
            return std::make_pair("", ErrorInfo::FromExplicitError(CancellationErrorCode::BadRequest, errorMessage.str()));
        }

        // This is a workaround for the issue that the service doesn't support empty language even it doesn't use this property at all.
        if (chosenLanguage.empty())
        {
            chosenLanguage = "en-US";
        }

        std::ostringstream oss;
        oss << "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xmlns:mstts='http://www.w3.org/2001/mstts' xmlns:emo='http://www.w3.org/2009/10/emotionml' xml:lang='";
        oss << chosenLanguage << "'>";
        if (!chosenVoice.empty())
        {
            oss << "<voice name='" << chosenVoice << "'>";
        }
        oss << XmlEncode(text);
        if (!chosenVoice.empty())
        {
            oss << "</voice>";
        }
        oss << "</speak>";

        return std::make_pair(oss.str(), nullptr);
    };

    static std::string XmlEncode(const std::string& text)
    {
        std::stringstream ss;
        for (char c : text)
        {
            if (c == '&')
            {
                ss << "&amp;";
            }
            else if (c == '<')
            {
                ss << "&lt;";
            }
            else if (c == '>')
            {
                ss << "&gt;";
            }
            else if (c == '\'')
            {
                ss << "&apos;";
            }
            else if (c == '"')
            {
                ss << "&quot;";
            }
            else
            {
                ss << c;
            }
        }

        return ss.str();
    };

    static std::string XmlDecode(const std::string& text)
    {
        auto replace_all = [](std::string& target, const std::string& from, const std::string& to)
        {
            std::size_t startPos = 0;
            while ((startPos = target.find(from, startPos)) != std::string::npos)
            {
                target.replace(startPos, from.length(), to);
                startPos += to.length();
            }
        };

        std::string result = text;
        replace_all(result, "&lt;", "<");
        replace_all(result, "&gt;", ">");
        replace_all(result, "&apos;", "'");
        replace_all(result, "&quot;", "\"");
        replace_all(result, "&amp;", "&");

        return result;
    }

    /*static CancellationErrorCode HttpStatusCodeToCancellationErrorCode(int httpStatusCode)
    {
        if (httpStatusCode < 400)
        {
            return CancellationErrorCode::NoError;
        }

        CancellationErrorCode errorCode = CancellationErrorCode::NoError;
        switch (httpStatusCode)
        {
        case 401:
            errorCode = CancellationErrorCode::AuthenticationFailure;
            break;

        case 400:
            errorCode = CancellationErrorCode::BadRequest;
            break;

        case 429:
            errorCode = CancellationErrorCode::TooManyRequests;
            break;

        case 403:
            errorCode = CancellationErrorCode::Forbidden;
            break;

        case 408:
        case 504:
            errorCode = CancellationErrorCode::ServiceTimeout;
            break;

        case 500:
        case 501:
        case 502:
        case 505:
        case 506:
        case 507:
        case 509:
        case 510:
        case 600:
            errorCode = CancellationErrorCode::ServiceError;
            break;

        case 503:
            errorCode = CancellationErrorCode::ServiceUnavailable;
            break;

        default:
            errorCode = CancellationErrorCode::ConnectionFailure;
            break;
        }

        return errorCode;
    }*/

    static SpxWAVEFORMATEX_Type GetSpeechSynthesisOutputFormatFromString(const std::string& formatStr)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, formatStr.data() == nullptr);

        if (formatStr == "raw-8khz-8bit-mono-mulaw" || formatStr == "riff-8khz-8bit-mono-mulaw")
        {
            return BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MULAW, 1, 8000, 8000, 1, 8, 0, nullptr);
        }
        else if (formatStr == "riff-16khz-16kbps-mono-siren" || formatStr == "audio-16khz-16kbps-mono-siren")
        {
            uint16_t extraData = 320;
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_SIREN, 1, 16000, 2000, 40, 0, 2, (uint8_t *)(&extraData));
        }
        else if (PAL::stricmp(formatStr.data(), "audio-16khz-32kbitrate-mono-mp3") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 16000, 32 << 7, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "audio-16khz-128kbitrate-mono-mp3") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 16000, 128 << 7, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "audio-16khz-64kbitrate-mono-mp3") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 16000, 64 << 7, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "audio-24khz-48kbitrate-mono-mp3") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 24000, 48 << 7, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "audio-24khz-96kbitrate-mono-mp3") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 24000, 96 << 7, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "audio-24khz-160kbitrate-mono-mp3") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 24000, 160 << 7, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "raw-16khz-16bit-mono-truesilk") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_SILK_SKYPE, 1, 16000, 32000, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "raw-24khz-16bit-mono-truesilk") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_SILK_SKYPE, 1, 24000, 48000, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "riff-16khz-16bit-mono-pcm") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "riff-24khz-16bit-mono-pcm") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 24000, 48000, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "raw-16khz-16bit-mono-pcm") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0, nullptr);
        }
        else if (PAL::stricmp(formatStr.data(), "raw-24khz-16bit-mono-pcm") == 0)
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 24000, 48000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "raw-8khz-16bit-mono-pcm" || formatStr == "riff-8khz-16bit-mono-pcm")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 8000, 16000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "ogg-16khz-16bit-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_OGG_OPUS, 1, 16000, 8000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "ogg-24khz-16bit-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_OGG_OPUS, 1, 24000, 8000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "raw-48khz-16bit-mono-pcm" || formatStr == "riff-48khz-16bit-mono-pcm")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 48000, 96000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "audio-48khz-96kbitrate-mono-mp3")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 48000, 96 << 7, 2, 16, 0, nullptr);
        }
        else if (formatStr == "audio-48khz-192kbitrate-mono-mp3")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_MP3, 1, 48000, 192 << 7, 2, 16, 0, nullptr);
        }
        else if (formatStr == "ogg-48khz-16bit-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_OGG_OPUS, 1, 48000, 12000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "webm-16khz-16bit-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_WEBM_OPUS, 1, 16000, 4000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "webm-24khz-16bit-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_WEBM_OPUS, 1, 24000, 6000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "raw-8khz-8bit-mono-alaw" || formatStr == "riff-8khz-8bit-mono-alaw")
        {
            return BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_ALAW, 1, 8000, 8000, 1, 8, 0, nullptr);
        }
        else if (formatStr == "webm-24khz-16bit-24kbps-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_WEBM_OPUS, 1, 24000, 3000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "audio-16khz-16bit-32kbps-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_OPUS, 1, 16000, 4000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "audio-24khz-16bit-48kbps-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_OPUS, 1, 24000, 6000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "audio-24khz-16bit-24kbps-mono-opus")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_OPUS, 1, 24000, 3000, 2, 16, 0, nullptr);
        }
        else if (formatStr == "raw-22050hz-16bit-mono-pcm" || formatStr == "riff-22050hz-16bit-mono-pcm")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0, nullptr);
        }
        else if (formatStr == "raw-44100hz-16bit-mono-pcm" || formatStr == "riff-44100hz-16bit-mono-pcm")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0, nullptr);
        }
        else if (formatStr == "amr-wb-16000hz")
        {
            // bitrate is 23.85 kbps
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_AMR_WB, 1, 16000, 3052, 2, 16, 0, nullptr);
        }
        else if (formatStr == "g722-16khz-64kbps")
        {
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_G722_TTS, 1, 16000, 8000, 2, 16, 0, nullptr);
        }
        // else if (formatStr == "audio-24khz-16bit-mono-flac")
        // {
        //     return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_FLAC, 1, 24000, 24000, 2, 16, 0, nullptr);
        // }
        // else if (formatStr == "audio-48khz-16bit-mono-flac")
        // {
        //     return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_FLAC, 1, 48000, 30000, 2, 16, 0, nullptr);
        // }
        else if (formatStr.empty())
        {
            // Set default format to riff-16khz-16bit-mono-pcm
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0, nullptr);
        }
        else
        {
            // if format string is set and not known, just pass it to service
            return CSpxSynthesisHelper::BuildSpeechSynthesisOutputFormat(WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0, nullptr);
        }
    }

    static SpxWAVEFORMATEX_Type BuildSpeechSynthesisOutputFormat( \
        uint16_t wFormatTag, uint16_t nChannels, uint32_t nSamplesPerSec, uint32_t nAvgBytesPerSec, \
        uint16_t nBlockAlign, uint16_t wBitsPerSample, uint16_t cbSize, uint8_t* extraData)
    {
        auto srcFormat = SPXWAVEFORMATEX{ wFormatTag, nChannels, nSamplesPerSec, nAvgBytesPerSec, nBlockAlign, wBitsPerSample, cbSize };
        uint16_t basicSize = sizeof(SPXWAVEFORMATEX);
        uint16_t requiredSize = basicSize + srcFormat.cbSize;
        auto format = SpxAllocWAVEFORMATEX(requiredSize);

        memcpy(format.get(), &srcFormat, basicSize); // Copy basic data
        if (cbSize > 0 && extraData != nullptr)
        {
            memcpy(format.get(), extraData, cbSize); // Copy extra data
        }

        return format;
    }

    static std::shared_ptr<std::vector<uint8_t>> BuildRiffHeader(uint32_t cData, uint32_t cEventData, SpxWAVEFORMATEX_Type audioFormat)
    {
        RIFFHDR riff(0);
        BLOCKHDR block(0);
        DATAHDR dataHdr(0);

        uint32_t cRiff = sizeof(riff);
        uint32_t cBlock = sizeof(block);
        uint32_t cWaveEx = 18 + audioFormat->cbSize; // use 18 for actual size to avoid compiler alignment difference.
        uint32_t cDataHdr = sizeof(dataHdr);

        uint32_t total = cRiff + cBlock + cWaveEx + cDataHdr;
        if (audioFormat->wFormatTag == WAVE_FORMAT_SIREN)
        {
            total += 12;
        }

        if (cEventData > 0)
        {
            total += (8 + cEventData);
        }

        uint8_t tmpBuf[128];
        uint8_t* p = tmpBuf;
        // Write the RIFF section
        riff._len = total + cData - 8/* - cRiff*/; // for the "WAVE" 4 characters
        buffer_write(&p, riff._id);
        buffer_write(&p, riff._len);
        buffer_write(&p, riff._type);

        // Write the wave header section
        block._len = cWaveEx;
        buffer_write(&p, block._id);
        buffer_write(&p, block._len);

        // Write the FormatEx structure
        buffer_write(&p, audioFormat->wFormatTag);
        buffer_write(&p, audioFormat->nChannels);
        buffer_write(&p, audioFormat->nSamplesPerSec);
        buffer_write(&p, audioFormat->nAvgBytesPerSec);
        buffer_write(&p, audioFormat->nBlockAlign);
        buffer_write(&p, audioFormat->wBitsPerSample);
        buffer_write(&p, audioFormat->cbSize);

        if (audioFormat->wFormatTag == WAVE_FORMAT_SIREN)
        {
            buffer_write(&p, static_cast<uint16_t>(320));
            buffer_write(&p, 'f');
            buffer_write(&p, 'a');
            buffer_write(&p, 'c');
            buffer_write(&p, 't');
            buffer_write(&p, static_cast<uint32_t>(4));
            uint32_t factSize = (cData * 320) / audioFormat->nBlockAlign;
            buffer_write(&p, factSize);
        }

        // Write the data section
        dataHdr._len = cData;
        buffer_write(&p, dataHdr._id);
        buffer_write(&p, dataHdr._len);

        return std::make_shared<std::vector<uint8_t>>(tmpBuf, p);
    }

    static bool LanguageAutoDetectionEnabled(const std::shared_ptr<ISpxNamedProperties>& properties)
    {
        const auto autoDetectSourceLanguages = properties->GetOr(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, "");
        return g_autoDetectSourceLang_OpenRange == autoDetectSourceLanguages;
    }
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
