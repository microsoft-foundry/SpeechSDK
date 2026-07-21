//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <signal.h>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <exception.h>

#include "time_utils.h"
#include "file_utils.h"

#if __APPLE__   // utility macros for Apple platforms selective test targeting
#include <TargetConditionals.h>
#endif

#ifdef _DEBUG
#define SPX_CONFIG_DBG_TRACE_ALL 1
#define SPX_CONFIG_TRACE_ALL 1
#else
#define SPX_CONFIG_TRACE_ALL 1
#endif

#if defined(__AZAC_DO_TRACE_IMPL) || defined(__SPX_DO_TRACE_IMPL)
#define SUPPRESS_CORE_COMMON_TRACE_IMPL
#endif
#if defined(__AZAC_THROW_HR_IMPL) || defined(__SPX_THROW_HR_IMPL)
#define SUPPRESS_CORE_COMMON_THROW_IMPL
#endif
#include "spxcore_common.h"

#include "speechapi_cxx_enums.h"
#include "string_utils.h"
#include "debug_utils.h"

#include "exception.h"
#include "spxdebug.h"
#include "platform.h"
#include "interfaces/i_web_socket_state.h"
#include "interfaces/i_http_endpoint_info.h"

#ifdef _MSC_VER
#pragma warning( push )
// disable: (9754,94): error 6330:  : 'const char' passed as _Param_(1) when 'unsigned char' is required in call to 'isalnum'.
#pragma warning( disable : 6330 )
#endif

// Configuration for the fetched Catch2 project.
// It's useful for us to include just what we use (and not <catch2/catch_all.hpp>) as doing so dramatically reduces
// build time, particularly for incremental builds.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_session.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_translate_exception.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "ajv.h"

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#if !defined(SPXTEST_PROVIDES_MAIN)
#define EXTERN extern
#else
#define EXTERN
#endif

constexpr const char* TEST_AUDIOUTTERANCES_FILE = "test.audio.utterances.json";
constexpr const char* TEST_DEFAULTS_FILE = "test.defaults.json";
constexpr const char* TEST_SUBSCRIPTIONSREGIONS_FILE = "test.subscriptions.regions.json";
constexpr const char* TEST_CERTIFICATE_FILE = "test.certificates.json";

using namespace std::chrono_literals;

// Catch2 does not have built-in concurrency protection. We'll work around that by using a scoped
// lock in the appropriate places in the wrapper macros to ensure we're synchronized.
static std::mutex g_catchConcurrencyMutex;
#define CATCH_AUTO_LOCK() std::lock_guard<std::mutex> catchAutoLock { g_catchConcurrencyMutex }

namespace Config
{
    EXTERN bool DoDiscover;
    EXTERN std::string MemoryLoggerExit;
    EXTERN std::string MemoryLoggerExitFile;
    EXTERN std::string OfflineModelPathList;
    EXTERN std::string OfflineModelName;
    EXTERN std::string OfflineModelKey;
}

std::string ResolvePath(const std::string& relativePath);

// Subscriptions and regions keys
constexpr const char* KEY = "Key";
constexpr const char* REGION = "Region";
constexpr const char* UNIFIED_SPEECH_SUBSCRIPTION = "UnifiedSpeechSubscription";
constexpr const char* CUSTOM_TRANSLATION_MODEL_SUBSCRIPTION = "CustomTranslationModelSubscription";
constexpr const char* SOURCE_LANGUAGE_RECOGNITION_SUBSCRIPTION = "SourceLanguageRecognitionSubscription";
constexpr const char* DIALOG_SUBSCRIPTION = "DialogSubscription";
constexpr const char* AAD_SPEECH_CLIENT_SECRET = "AADSpeechClientSecret";
constexpr const char* CONVERSATION_TRANSCRIPTION_TEAMS_SUBSCRIPTION = "ConversationTranscriptionTeamsSubscription";
constexpr const char* CUSTOM_VOICE_SUBSCRIPTION = "CustomVoiceSubscription";
constexpr const char* PERSONAL_VOICE_SUBSCRIPTION = "PersonalVoiceSubscription";

// Default settings keys
constexpr const char* ENDPOINT = "Endpoint";
constexpr const char* DIALOG_FUNCTIONAL_TEST_BOT = "DialogFunctionalTestBot";
constexpr const char* DIALOG_CUSTOM_COMMANDS_APP_ID = "DialogCustomCommandsAppId";
constexpr const char* LONG_RUNNING = "LongRunning";
constexpr const char* DEPLOYMENT_ID = "DeploymentId";
constexpr const char* CONVERSATION_TRANSCRIPTION_TEAMS_ENDPOINT = "ConversationTranscriptionTeamsEndpoint";
constexpr const char* INPUT_DIR = "InputDir";
constexpr const char* CUSTOM_VOICE_DEPLOYMENT_ID = "CustomVoiceDeploymentId";
constexpr const char* CUSTOM_VOICE_VOICE_NAME = "CustomVoiceVoiceName";
constexpr const char* PERSONAL_VOICE_NAME = "PersonalVoiceName";
constexpr const char* OFFLINE_SPEECH_SYNTHESIS_MODEL_PATH = "OfflineSpeechSynthesisModelPath";
constexpr const char* OFFLINE_SPEECH_SYNTHESIS_MODEL_KEY = "OfflineSpeechSynthesisModelKey";
constexpr const char* OFFLINE_SPEECH_SYNTHESIS_LICENSE = "OfflineSpeechSynthesisLicense";
constexpr const char* OFFLINE_SPEECH_RECOGNITION_MODEL_PATH = "OfflineSpeechRecognitionModelPath";
constexpr const char* OFFLINE_SPEECH_RECOGNITION_DEFAULT_MODEL_NAME = "OfflineSpeechRecognitionDefaultModelName";
constexpr const char* OFFLINE_SPEECH_TRANSLATION_MODEL_PATH = "OfflineSpeechTranslationModelPath";
constexpr const char* OFFLINE_SPEECH_TRANSLATION_MANY_TO_EN_MODEL_NAME = "OfflineSpeechTranslationManyToEnModelName";
constexpr const char* OFFLINE_SPEECH_TRANSLATION_EN_TO_MANY_MODEL_NAME = "OfflineSpeechTranslationEnToManyModelName";
constexpr const char* MULTIKEYWORD_RECOGNITION_MODEL_PATH = "MultiKeywordRecognitionModelPath";
constexpr const char* MULTIKEYWORD_RECOGNITION_MODEL_NAME = "MultiKeywordRecognitionModelName";
constexpr const char* TRANSLATION_TEST_CATEGORY_ID = "TranslationTestCategoryId";
constexpr const char* SPEECH_AAD_ENDPOINT = "SpeechAADEndpoint";

// Related to audio test files, utterances and keys
constexpr const char* FILE_PATH = "FilePath";
constexpr const char* NATIVE_LANGUAGE = "NativeLanguage";
constexpr const char* UTTERANCES = "Utterances";
constexpr const char* UTTERANCE_TEXT = "Text";
constexpr const char* LEXICAL_TEXT = "LexicalText";
constexpr const char* PROFANITY_RAW = "ProfanityRaw";
constexpr const char* PROFANITY_MASKED = "ProfanityMasked";
constexpr const char* PROFANITY_MASKED_PATTERN = "ProfanityMaskedPattern";
constexpr const char* PROFANITY_REMOVED = "ProfanityRemoved";
constexpr const char* PROFANITY_TAGGED = "ProfanityTagged";
constexpr const char* UTTERANCE_SSML = "Ssml";
constexpr const char* VOICE_NAME = "VoiceName";
constexpr const char* AUDIO_OFFSETS = "AudioOffsets";
constexpr const char* AUDIO_DURATION = "AudioDuration";
constexpr const char* TEXT_OFFSETS = "TextOffsets";
constexpr const char* SSML_OFFSETS = "SsmlOffsets";
constexpr const char* WORD_LENGTHS = "WordLengths";
constexpr const char* TEXT_BOUNDARY_TEXT = "TextBoundaryText";
constexpr const char* TEXT_BOUNDARY_TYPES = "TextBoundaryTypes";
constexpr const char* VISEME_IDS = "VisemeIds";
constexpr const char* BOOKMARKS = "Bookmarks";
constexpr const char* OFFLINE_SYNTHESIZED_FILE_PATH = "OfflineSynthesizedFilePath";

//Related to audio certificates
constexpr const char* DIGI_CERTIFICATE = "certificate_pem";

constexpr const char* SINGLE_UTTERANCE_ENGLISH = "SingleUtteranceEnglish";
constexpr const char* SINGLE_UTTERANCE_ENGLISH_8KHZ = "SingleUtteranceEnglish8kHz";
constexpr const char* SINGLE_UTTERANCE_ENGLISH_8KHZ_2CH = "SingleUtteranceEnglish8kHz2Channels";
constexpr const char* SINGLE_UTTERANCE_ENGLISH_FIVE_POINT_ONE_CHANNELS = "SingleUtteranceEnglishFivePointOneChannels";
constexpr const char* LONGER_SINGLE_UTTERANCE_ENGLISH = "LongerSingleUtteranceEnglish";
constexpr const char* SINGLE_UTTERANCE_ENGLISH_WITH_SEGMENTATION_GAP = "SingleUtteranceEnglishWithSegmentationGap";
constexpr const char* CORRECTIONS_UTTERANCE = "CorrectionsUtterance";
constexpr const char* SINGLE_UTTERANCE_CHINESE = "SingleUtteranceChinese";
constexpr const char* MULTIPLE_UTTERANCE_CHINESE = "MultipleUtteranceChinese";
constexpr const char* SINGLE_UTTERANCE_MP3 = "SingleUtteranceMP3";
constexpr const char* SINGLE_UTTERANCE_MP4 = "SingleUtteranceMP4";
constexpr const char* SINGLE_UTTERANCE_OPUS = "SingleUtteranceOPUS";
constexpr const char* SINGLE_UTTERANCE_A_LAW = "SingleUtteranceALaw";
constexpr const char* SINGLE_UTTERANCE_MU_LAW = "SingleUtteranceMULaw";
constexpr const char* SINGLE_UTTERANCE_FLAC = "SingleUtteranceFLAC";
constexpr const char* SINGLE_UTTERANCE_G722 = "SingleUtteranceG722";
constexpr const char* SINGLE_UTTERANCE_3X = "SingleUtterance3x";
constexpr const char* SINGLE_UTTERANCE_MULTIPLE_TURNS = "SingleUtteranceMultipleTurns";
constexpr const char* SINGLE_UTTERANCE_CATALAN = "SingleUtteranceCatalan";
constexpr const char* MULTIPLE_UTTERANCE_ENGLISH = "MultipleUtteranceEnglish";
constexpr const char* AUDIO_44_1KHZ = "Audio441Khz";
constexpr const char* AUDIO_11_KHZ = "Audio11Khz";
constexpr const char* AUDIO_32BIT_TEST_TEST = "Audio32BitTestTest";
constexpr const char* HEY_CORTANA = "HeyCortana";
constexpr const char* SINGLE_UTTERANCE_GERMAN = "SingleUtteranceGerman";
constexpr const char* SINGLE_UTTERANCE_FRENCH = "SingleUtteranceFrench";
constexpr const char* MULTILINGUAL_UTTERANCE = "MultiLingualUtteranceEnglishJapanese";
constexpr const char* MULTILINGUAL_UTTERANCE2 = "MultiLingualUtteranceSpanishEnglish";
constexpr const char* INTENT_UTTERANCE = "IntentUtterance";
constexpr const char* AMBIGUOUS_SPEECH = "AmbiguousSpeech";
constexpr const char* AMBIGUOUS_SPEECH2 = "AmbiguousSpeech2";
constexpr const char* AMBIGUOUS_SPEECH3 = "AmbiguousSpeech3";
constexpr const char* NO_PAUSE_SPEECH_ENGLISH = "NoPauseSpeechEnglish";
constexpr const char* COMPUTER_KEYWORD_WITH_SINGLE_UTTERANCE_1 = "ComputerKeywordWithSingleUtterance1"; // Used to be accept
constexpr const char* COMPUTER_KEYWORD_WITH_SINGLE_UTTERANCE_2 = "ComputerKeywordWithSingleUtterance2";
constexpr const char* COMPUTER_KEYWORD_WITH_SINGLE_UTTERANCE_2X = "ComputerKeywordWithSingleUtterance2x"; // Used to be acceptx2
constexpr const char* COMPUTER_KEYWORD_WITH_SINGLE_UTTERANCE_3 = "ComputerKeywordWithSingleUtterance3";
constexpr const char* COMPUTER_KEYWORD_WITH_MULTIPLE_TURNS_1 = "ComputerKeywordWithMultipleTurns";
constexpr const char* COMPUTER_KEYWORD_WITH_MULTIPLE_TURNS_2X = "ComputerKeywordWithMultipleTurns2x";
constexpr const char* COMPUTER_KEYWORD_WITH_MULTIPLE_TURNS_3X = "ComputerKeywordWithMultipleTurns3x";
constexpr const char* COMPUTER_KEYWORD_WITH_MULTIPLE_TURNS_30X = "ComputerKeywordWithMultipleTurns30x";
constexpr const char* MULTIKEYWORD_WITH_MULTIPLE_TURNS = "MultiKeywordWithMultipleTurns";
constexpr const char* SECRET_KEYWORDS = "SecretKeywords";
constexpr const char* CONVERSATION_BETWEEN_TWO_PERSONS_ENGLISH = "ConversationBetweenTwoPersonsEnglish";
constexpr const char* CONVERSATION_BETWEEN_TWO_PERSONS_ENGLISH_MONO = "ConversationBetweenTwoPersonsEnglishMono";
constexpr const char* CONVERSATION_BETWEEN_TWO_PERSONS_ENGLISH_MONO_REPEATED = "ConversationBetweenTwoPersonsEnglishMonoRepeated";
constexpr const char* PERSON_ENROLLMENT_ENGLISH_1 = "PersonEnrollmentEnglish1";
constexpr const char* PERSON_ENROLLMENT_ENGLISH_2 = "PersonEnrollmentEnglish2";
constexpr const char* SINGLE_UTTERANCE_WITH_SPECIAL_CHARACTER = "SingleUtteranceWithSpecialCharacter";
constexpr const char* SHORT_SILENCE = "ShortSilence";
constexpr const char* SINGLE_UTTERANCE_WITH_PUNCTUATION = "SingleUtteranceWithPunctuation";
constexpr const char* PROFANITY_SINGLE_UTTERANCE_ENGLISH_1 = "ProfanitySingleUtteranceEnglish1";
constexpr const char* PROFANITY_SINGLE_UTTERANCE_ENGLISH_2 = "ProfanitySingleUtteranceEnglish2";
constexpr const char* PROFANITY_SINGLE_UTTERANCE_ENGLISH_3 = "ProfanitySingleUtteranceEnglish3";
constexpr const char* PROFANITY_SINGLE_UTTERANCE_GERMAN = "ProfanitySingleUtteranceGerman";
constexpr const char* PRONUNCIATION_ASSESSMENT_BAD_PRONUNCIATION = "PronunciationAssessmentBadPronunciation";
constexpr const char* PRONUNCIATION_ASSESSMENT_GOOD_PRONUNCIATION_CHINESE = "PronunciationAssessmentGoodPronunciationChinese";
constexpr const char* PRONUNCIATION_ASSESSMENT_FALL = "PronunciationAssessmentFall";
constexpr const char* VAD_SILENCE_AND_NOISE_NON_VOICE = "VoiceActivityDetectionNonVoiceAudio";
constexpr const char* EM_DASH = "EmDash";
constexpr const char* EMPTY_WAV_FILE = "EmptyWavFile";
constexpr const char* NON_LATIN_FILE_NAME = "NonLatinFileName";
constexpr const char* BAD_HEADER = "BadHeader";
constexpr const char* AUDIO_FOR_CUSTOM_MT = "AudioForCustomMT";


constexpr const char* SYNTHESIS_WORD_BOUNDARY_UTTERANCE_CHINESE = "SynthesisWordBoundaryUtteranceChinese";
constexpr const char* SYNTHESIS_UTTERANCE_ENGLISH = "SynthesisUtteranceEnglish";
constexpr const char* SYNTHESIS_SHORT_UTTERANCE_CHINESE = "SynthesisShortUtteranceChinese";
constexpr const char* SYNTHESIS_UTTERANCE_CHINESE_1 = "SynthesisUtteranceChinese1";
constexpr const char* SYNTHESIS_UTTERANCE_CHINESE_2 = "SynthesisUtteranceChinese2";
constexpr const char* SYNTHESIS_LONG_UTTERANCE = "SynthesisLongUtterance";
constexpr const char* SYNTHESIS_UTTERANCE_FOR_WORD_BOUNDARY = "SynthesisUtteranceForWordBoundary";
constexpr const char* SYNTHESIS_UTTERANCE_FOR_VISEME = "SynthesisUtteranceForViseme";
constexpr const char* SYNTHESIS_UTTERANCE_FOR_BOOKMARK = "SynthesisUtteranceForBookmark";
constexpr const char* SYNTHESIS_UTTERANCE_SSML = "SynthesisUtteranceSSML";
constexpr const char* SYNTHESIS_UTTERANCE_FOR_TEXT_STREAMING = "SynthesisUtteranceForTextStreaming";


EXTERN std::map<std::string, std::string> DefaultSettingsMap;

struct SubscriptionRegion
{
    std::string Key;
    std::string Region;
    std::string Endpoint;
};

struct Utterance
{
    std::string Text;
    std::string LexicalText;
    std::string ProfanityRaw;
    std::string ProfanityMasked;
    std::string ProfanityMaskedPattern;
    std::string ProfanityRemoved;
    std::string ProfanityTagged;
    std::string Ssml;
    std::string VoiceName;
    std::vector<uint64_t> AudioOffsets;
    int AudioDuration;
    std::vector<int> TextOffsets;
    std::vector<int> SsmlOffsets;
    std::vector<int> WordLengths;
    std::vector<std::string> TextBoundaryText;
    std::vector<Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType> TextBoundaryTypes;
    std::vector<int> VisemeIds;
    std::vector<std::string> Bookmarks;
    std::string OfflineSynthesizedFilePath;
};

struct AudioEntry
{
    std::string FilePath;
    std::string NativeLanguage;
    std::map<std::string, std::vector<Utterance>> Utterances;

    std::string GetRootRelativePath() const
    {
        return DefaultSettingsMap[INPUT_DIR] + "/" + FilePath;
    }

    std::vector<std::string> GetExpectedRecognitions(const char* lang = nullptr) const
    {
        if (lang == nullptr)
        {
            lang = NativeLanguage.c_str();
        }

        auto found = Utterances.find(lang);
        if (found == Utterances.end())
        {
            found = Utterances.find(PAL::StringUtils::ToLower(lang));
            if (found == Utterances.end())
            {
                return {};
            }
        }

        std::vector<std::string> recos;
        for (const auto & ut : found->second)
        {
            recos.push_back(ut.Text);
        }

        return recos;
    }
};

EXTERN std::map<std::string, AudioEntry> AudioUtterancesMap;
EXTERN std::map<std::string, SubscriptionRegion> SubscriptionsRegionsMap;
EXTERN std::map<std::string, std::string> CertMap;

inline std::string ROOT_RELATIVE_PATH(const std::string& pathName)
{
    return DefaultSettingsMap[INPUT_DIR] + "/" + AudioUtterancesMap[pathName].FilePath;
}

inline bool exists(const std::string& name) {
    return std::ifstream(name.c_str()).good();
}

inline size_t file_size(const std::string& name)
{
    auto f = std::ifstream(name.c_str());
    f.seekg(0, std::ios::end);

    return f.good()
        ? static_cast<size_t>(f.tellg())
        : 0;
}

inline std::ifstream get_stream(const std::string& name) {
    return std::ifstream(name.c_str(), std::ifstream::binary);
}

inline void test_diagnostics_log_trace_message(int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, ...)
{
    char sz[4096];
    va_list argptr;

    va_start(argptr, pszFormat);
    diagnostics_log_format_message(sz, 4096, level, pszTitle, fileName, lineNumber, pszFormat, argptr);
    va_end(argptr);

     fprintf(stderr, "TEST: %s", sz);

     va_start(argptr, pszFormat);
     diagnostics_log_trace_message2(level, pszTitle, fileName, lineNumber, pszFormat, argptr);
     va_end(argptr);
}

#define SPX_TEST_TRACE_ERROR(file, title, line, options, msg, ...) __SPX_TRACE_ERROR((std::string("SPX_TRACE_ERROR: ") + std::string(title)).c_str(), file, line, msg "%s", ##__VA_ARGS__, "")
#define SPX_TEST_TRACE_INFO(file, title, line, options, msg, ...)  __SPX_TRACE_INFO((std::string("SPX_TRACE_INFO: ") + std::string(title)).c_str(), file, line, msg "%s", ##__VA_ARGS__, "")

#undef max
#undef min

class StringComparisions
{
public:
    static bool AssertFuzzyMatch(const std::string received, const std::string expected, size_t deltaPercentage = 10, size_t minAllowedMismatchCount = 1)
    {
        size_t errors, totalWords;

        CalculateWordErrorRate(received, expected, &errors, &totalWords);

        // Find the max number of errors to allow.
        size_t allowedEdits = std::max(minAllowedMismatchCount, (deltaPercentage * totalWords) / 100);

        return (allowedEdits >= errors);
    }

private:
    template <class T>
    static unsigned int Levenshtein(const T& s1, const T& s2)
    {
        const std::size_t l1 = s1.size(), l2 = s2.size();
        std::vector<unsigned int> d1(l2 + 1), d2(l2 + 1);

        for (unsigned int i = 0; i < d2.size(); i++) d2[i] = i;
        for (unsigned int i = 0; i < l1; i++) {
            d1[0] = i + 1;
            for (unsigned int j = 0; j < l2; j++) {
                d1[j + 1] = std::min({ d2[1 + j] + 1, d1[j] + 1, d2[j] + (s1[i] == s2[j] ? 0 : 1) });
            }
            d1.swap(d2);
        }
        return d2[l2];
    }

    static void CalculateWordErrorRate(const std::string& text1, const std::string& text2, size_t* errors, size_t* words)
    {
        auto words1 = PAL::split(text1, ' ');
        auto words2 = PAL::split(text2, ' ');

        *errors = Levenshtein(words1, words2);
        *words = words1.size();
    }
};

namespace Catch
{
    /// <summary>
    /// Helper class to check that exceptions contains specific messages
    /// </summary>
    class ExceptionMessageContainsMatcher : public Catch::Matchers::MatcherBase<std::exception>
    {
    private:
        Catch::Matchers::CasedString m_expectedString;

    public:
        ExceptionMessageContainsMatcher(const std::string& contains, bool caseSensitive) :
            m_expectedString(contains, caseSensitive ? Catch::CaseSensitive::Yes : Catch::CaseSensitive::No)
        {}

        virtual bool match(const std::exception& ex) const override
        {
            std::string exStr(m_expectedString.adjustString(ex.what()));
            size_t pos = exStr.find(m_expectedString.m_str, 0);
            return pos != std::string::npos;
        }

        virtual std::string describe() const override
        {
            std::ostringstream ss;
            ss << "Exception message contains \"" << m_expectedString.m_str
                << "\" " << m_expectedString.caseSensitivitySuffix();
            return ss.str();
        }
    };

    inline ExceptionMessageContainsMatcher MessageContainsHR(SPXHR hr)
    {
        return ExceptionMessageContainsMatcher(Microsoft::CognitiveServices::Speech::Impl::stringify(hr), false);
    }

    inline ExceptionMessageContainsMatcher MessageContains(const std::string& str, bool caseSensitive = true)
    {
        return ExceptionMessageContainsMatcher(str, caseSensitive);
    }

    class ExceptionWithCallStackMatcher : public Catch::Matchers::MatcherBase<Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack>
    {
    protected:
        using ExceptionWithCallStack = Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack;

        struct Comp
        {
            std::string description;
            std::function<bool(const ExceptionWithCallStack&)> predicate;
        };

    private:
        std::vector<Comp> m_matchers;

    public:
        ExceptionWithCallStackMatcher() = default;

        ExceptionWithCallStackMatcher(const Comp& matcher) :
            ExceptionWithCallStackMatcher({ matcher })
        {}

        ExceptionWithCallStackMatcher(std::initializer_list<Comp> matchers) :
            m_matchers(matchers)
        {}

        virtual bool match(const ExceptionWithCallStack& ex) const override
        {
            return m_matchers.size() > 0
                && std::all_of(m_matchers.begin(), m_matchers.end(), [&ex](const Comp& c) { return c.predicate(ex); });
        }

    protected:
        virtual std::string describe() const override
        {
            std::ostringstream oss;
            oss << "==> Expected an ExceptionWithCallStack with: ";
            for (size_t i = 0; i < m_matchers.size(); i++)
            {
                if (i != 0) oss << ",";
                oss << " " << m_matchers[i].description;
            }

            return oss.str();
        }
    };

    inline ExceptionWithCallStackMatcher ExceptionWithHR(SPXHR hr)
    {
        return ExceptionWithCallStackMatcher(
        {
            "HR equal to " + Microsoft::CognitiveServices::Speech::Impl::stringify(hr),
            [hr](const Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack& ex) -> bool
            {
                return ex.GetErrorCode() == hr;
            }
        });
    }

    template<typename TStringMatcher>
    inline ExceptionWithCallStackMatcher ExceptionWithHR(SPXHR hr, TStringMatcher messageMatcher)
    {
        return ExceptionWithCallStackMatcher(
        {
            {
                "HR equal to " + Microsoft::CognitiveServices::Speech::Impl::stringify(hr),
                [hr](const Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack& ex) -> bool
                {
                    return ex.GetErrorCode() == hr;
                }
            },
            {
                "Message " + messageMatcher.describe(),
                [messageMatcher](const Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack& ex) -> bool
                {
                    return messageMatcher.match(std::string(ex.what()));
                }
            }
        });
    }

    template<typename TStringMatcher>
    inline ExceptionWithCallStackMatcher ExceptionWithCallstack(TStringMatcher callStackMatcher)
    {
        return ExceptionWithCallStackMatcher(
        {
            "Call stack " + callStackMatcher.describe(),
            [callStackMatcher](const Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack& ex) -> bool
            {
                return callStackMatcher.match(std::string(ex.GetCallStack()));
            }
        });
    }

    /// <summary>
    /// Helper class to integrate fuzzy string matcher into the Catch framework. This integrates with the Catch
    /// framework and gives you richer error messages in case of assertion failures
    /// </summary>
    class FuzzyStringMatcher : public Catch::Matchers::MatcherBase<std::string>
    {
    private:
        const std::string m_expectedString;
        const size_t m_deltaPercentage;
        const size_t m_minMismatchCount;

    public:
        FuzzyStringMatcher(const std::string& expected, size_t deltaPercentage, size_t minAllowedMismatchCount)
            : m_expectedString(expected), m_deltaPercentage(deltaPercentage), m_minMismatchCount(minAllowedMismatchCount)
        {}

        virtual bool match(const std::string& str) const override
        {
            return ::StringComparisions::AssertFuzzyMatch(str, m_expectedString, m_deltaPercentage, m_minMismatchCount);
        }

        virtual std::string describe() const override
        {
            std::ostringstream oss;
            oss << "fuzzy string match against '" << m_expectedString << "' "
                << "with " << m_deltaPercentage << "% and "
                << m_minMismatchCount << " allowed mismatch";
            return oss.str();
        }
    };

    /// <summary>
    /// Helper method to make working with the fuzzy matcher easier. This allows you do assert like this:
    /// REQUIRE_THAT(finalRecognition, Catch::FuzzyMatch("This is a short test"));
    /// Should this fail, the Catch framework will automatically show you a string describing the error
    /// </summary>
    /// <param name="expected">The expected string</param>
    /// <param name="deltaPercentage">(Optional) The percentage difference allowed (0-100)</param>
    /// <param name="minAllowedMismatchCount">(Optional) The minimum allowed mismatch count</param>
    /// <returns>An instance of the fuzzy matcher</returns>
    inline FuzzyStringMatcher FuzzyMatch(const std::string& expected, size_t deltaPercentage = 10, size_t minAllowedMismatchCount = 1)
    {
        return FuzzyStringMatcher(expected, deltaPercentage, minAllowedMismatchCount);
    }

    /// <summary>
    /// Helper method since there is some odd template resolution failure when trying to do string
    /// contains
    /// </summary>
    /// <param name="str">The string that could be contained</param>
    /// <param name="caseSensitive">(Optional) True to do a case sensitive match</param>
    /// <returns>The string matcher to use</returns>
    inline Catch::Matchers::StringContainsMatcher ContainsString(const std::string& str, bool caseSensitive = true)
    {
        return Catch::Matchers::StringContainsMatcher(
            Catch::Matchers::CasedString(str, caseSensitive ? CaseSensitive::Yes : CaseSensitive::No ));
    }

    template<>
    struct StringMaker<Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack>
    {
        static std::string convert(Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack ex)
        {
            return "[" + Microsoft::CognitiveServices::Speech::Impl::stringify(ex.GetErrorCode())
                + "] " + ex.what() + ex.GetCallStack();
        }
    };
}

inline void from_json(const ajv::JsonReader& reader, std::map<std::string, SubscriptionRegion>& subscriptionsRegionsMap)
{
    for (auto name = reader.FirstName(); name.IsOk(); name++)
    {
        auto nameStr = name.AsString();
        auto item = reader[nameStr.c_str()];

        try {
            subscriptionsRegionsMap[nameStr].Key = item[KEY].AsString();
        }
        catch (std::exception& exception) {
            SPX_TEST_TRACE_ERROR(__FILE__, "from_json", __LINE__, 0, "SubscriptionRegion - failed to find key, %s", exception.what());
            throw;
        }

        try {
            subscriptionsRegionsMap[nameStr].Region = item[REGION].AsString();
        }
        catch (std::exception& exception) {
            SPX_TEST_TRACE_ERROR(__FILE__, "from_json", __LINE__, 0, "SubscriptionRegion - failed to find region, %s", exception.what());
            throw;
        }

        // This is an optional field in the JSON. But if it exists, it must have a value
        if (item[ENDPOINT].IsString())
        {
            try {
                subscriptionsRegionsMap[nameStr].Endpoint = item[ENDPOINT].AsString();
            }
            catch (std::exception& exception) {
                SPX_TEST_TRACE_ERROR(__FILE__, "from_json", __LINE__, 0, "SubscriptionRegion - failed to find endpoint, %s", exception.what());
                throw;
            }
        }
    }
}

inline void from_json(const ajv::JsonReader& reader, std::map<std::string, std::string>& defaultSettingsMap)
{
    for (auto name = reader.FirstName(); name.IsOk(); name++)
    {
        auto nameStr = name.AsString();
        auto item = reader[nameStr.c_str()];
        defaultSettingsMap[nameStr] = item.AsString();
    }
}

inline void from_json(const ajv::JsonReader& reader, std::map<std::string, std::vector<Utterance>>& utterances)
{
    for (auto name = reader.FirstName(); name.IsOk(); name++)
    {
        auto nameStr = name.AsString();
        utterances.insert(std::make_pair(nameStr, std::vector<Utterance>()));

        auto item = reader[nameStr.c_str()];
        auto utteranceCount = item.ValueCount();
        utterances[nameStr].resize(static_cast<size_t>(utteranceCount));

        for (int i = 0; i < utteranceCount; i++)
        {
            Utterance* utterance = &utterances[nameStr][i];

            if (item[i][UTTERANCE_TEXT].IsString())
            {
                utterance->Text = item[i][UTTERANCE_TEXT].AsString();
            }

            if (item[i][LEXICAL_TEXT].IsString())
            {
                utterance->LexicalText = item[i][LEXICAL_TEXT].AsString();
            }

            if (item[i][PROFANITY_RAW].IsString())
            {
                utterance->ProfanityRaw = item[i][PROFANITY_RAW].AsString();
            }

            if (item[i][PROFANITY_MASKED].IsString())
            {
                utterance->ProfanityMasked = item[i][PROFANITY_MASKED].AsString();
            }

            if (item[i][PROFANITY_MASKED_PATTERN].IsString())
            {
                utterance->ProfanityMaskedPattern = item[i][PROFANITY_MASKED_PATTERN].AsString();
            }

            if (item[i][PROFANITY_REMOVED].IsString())
            {
                utterance->ProfanityRemoved = item[i][PROFANITY_REMOVED].AsString();
            }

            if (item[i][PROFANITY_TAGGED].IsString())
            {
                utterance->ProfanityTagged = item[i][PROFANITY_TAGGED].AsString();
            }

            if (item[i][UTTERANCE_SSML].IsString())
            {
                utterance->Ssml = item[i][UTTERANCE_SSML].AsString();
            }

            if (item[i][VOICE_NAME].IsString())
            {
                utterance->VoiceName = item[i][VOICE_NAME].AsString();
            }

            if (item[i][AUDIO_OFFSETS].IsArray())
            {
                auto array = item[i][AUDIO_OFFSETS].AsArray();
                auto length = array.ValueCount();
                utterance->AudioOffsets.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    utterance->AudioOffsets[j] = array[j].AsUint64();
                }
            }

            if (item[i][AUDIO_DURATION].IsNumber())
            {
                utterance->AudioDuration = item[i][AUDIO_DURATION].AsInt();
            }
            else
            {
                utterance->AudioDuration = -1;
            }

            if (item[i][TEXT_OFFSETS].IsArray())
            {
                auto array = item[i][TEXT_OFFSETS].AsArray();
                auto length = array.ValueCount();
                utterance->TextOffsets.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    utterance->TextOffsets[j] = array[j].AsInt();
                }
            }

            if (item[i][SSML_OFFSETS].IsArray())
            {
                auto array = item[i][SSML_OFFSETS].AsArray();
                auto length = array.ValueCount();
                utterance->SsmlOffsets.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    utterance->SsmlOffsets[j] = array[j].AsInt();
                }
            }

            if (item[i][WORD_LENGTHS].IsArray())
            {
                auto array = item[i][WORD_LENGTHS].AsArray();
                auto length = array.ValueCount();
                utterance->WordLengths.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    utterance->WordLengths[j] = array[j].AsInt();
                }
            }

            if (item[i][TEXT_BOUNDARY_TEXT].IsArray())
            {
                auto array = item[i][TEXT_BOUNDARY_TEXT].AsArray();
                auto length = array.ValueCount();
                utterance->TextBoundaryText.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    utterance->TextBoundaryText[j] = array[j].AsString();
                }
            }

            if (item[i][TEXT_BOUNDARY_TYPES].IsArray())
            {
                auto array = item[i][TEXT_BOUNDARY_TYPES].AsArray();
                auto length = array.ValueCount();
                utterance->TextBoundaryTypes.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    auto str = array[j].AsString();

                    if (str == "Word")
                    {
                        utterance->TextBoundaryTypes[j] = Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType::Word;
                    }
                    else if (str == "Punctuation")
                    {
                        utterance->TextBoundaryTypes[j] = Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType::Punctuation;
                    }
                    else if (str == "Sentence")
                    {
                        utterance->TextBoundaryTypes[j] = Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType::Sentence;
                    }
                    else
                    {
                        throw std::runtime_error("Unknown SpeechSynthesisBoundaryType for " + nameStr + ": " + str);
                    }
                }
            }

            if (item[i][VISEME_IDS].IsArray())
            {
                auto array = item[i][VISEME_IDS].AsArray();
                auto length = array.ValueCount();
                utterance->VisemeIds.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    utterance->VisemeIds[j] = array[j].AsInt();
                }
            }

            if (item[i][BOOKMARKS].IsArray())
            {
                auto array = item[i][BOOKMARKS].AsArray();
                auto length = array.ValueCount();
                utterance->Bookmarks.resize(static_cast<size_t>(length));

                for (int j = 0; j < length; j++)
                {
                    utterance->Bookmarks[j] = array[j].AsString();
                }
            }

            if (item[i][OFFLINE_SYNTHESIZED_FILE_PATH].IsString())
            {
                utterance->OfflineSynthesizedFilePath = item[i][OFFLINE_SYNTHESIZED_FILE_PATH].AsString();
            }
        }
    }
}

inline void from_json(const ajv::JsonReader& reader, std::map<std::string, AudioEntry>& audioUtterancesMap)
{
    for (auto name = reader.FirstName(); name.IsOk(); name++)
    {
        auto nameStr = name.AsString();
        auto item = reader[nameStr.c_str()];

        if (item[FILE_PATH].IsString())
        {
            audioUtterancesMap[nameStr].FilePath = item[FILE_PATH].AsString();
        }
        else
        {
            audioUtterancesMap[nameStr].FilePath = "";
        }

        if (item[NATIVE_LANGUAGE].IsString())
        {
            audioUtterancesMap[nameStr].NativeLanguage = item[NATIVE_LANGUAGE].AsString();
        }
        else
        {
            audioUtterancesMap[nameStr].NativeLanguage = "";
        }

        if (!item[UTTERANCES].IsEmpty())
        {
            auto utteranceReader = item[UTTERANCES];
            from_json(utteranceReader, audioUtterancesMap[nameStr].Utterances);
        }
        else
        {
            audioUtterancesMap[nameStr].Utterances.clear();
        }
    }
}

class ConfigSettings {
private:
    static std::string getJson(std::string path)
    {
        ajv::JsonBuilder ajvJson;

        SPX_TEST_TRACE_INFO(__FILE__, "getJson", __LINE__, 0, "Loading json from %s", path.c_str());

        if (exists(path))
        {
            std::string content;

            try {
                std::ifstream file(path, std::ios::binary | std::ios::ate);
                if (!file.is_open())
                {
                    throw std::runtime_error("Could not open file: " + path);
                }

                // Get file size
                std::streamsize size = file.tellg();
                if (size < 0)
                {
                    throw std::runtime_error("Failed to read file size: " + path);
                }
                file.seekg(0, std::ios::beg);
                content.resize(static_cast<size_t>(size));

                // Read content
                if (!file.read(&content[0], size))
                {
                    throw std::runtime_error("Failed to read file content: " + path);
                }
            }
            catch (const std::exception& e)
            {
                SPX_TEST_TRACE_ERROR(__FILE__, "getJson", __LINE__, 0, "exception - %s", e.what());
            }

            ajvJson = ajv::json::Build(content);
        }
        else
        {
            SPX_TEST_TRACE_INFO(__FILE__, "getJson", __LINE__, 0, "json file %s cannot be found", path.c_str());
        }

        return ajvJson.AsJson();
    }

public:
    static void LoadFromJsonFile(std::string rootPathString)
    {
        SPX_TEST_TRACE_INFO(__FILE__, "LoadFromJsonFile", __LINE__, 0, "rootPathString is %s", rootPathString.c_str());

        std::string testSubscriptionsRegionsPath = rootPathString + TEST_SUBSCRIPTIONSREGIONS_FILE;
        std::string testAudioUtterancesPath = rootPathString + TEST_AUDIOUTTERANCES_FILE;
        std::string testDefaultsPath = rootPathString + TEST_DEFAULTS_FILE;

        auto subscriptionRegionData = getJson(testSubscriptionsRegionsPath);
        if (!subscriptionRegionData.empty())
        {
            auto parser = ajv::json::Parse(subscriptionRegionData);
            auto reader = parser.Reader();

            if (reader.IsObject())
            {
                from_json(reader, SubscriptionsRegionsMap);
            }
        }
        else
        {
            SPX_TEST_TRACE_ERROR(__FILE__, "LoadFromJsonFile", __LINE__, 0, "JSON could not be loaded from %s", testSubscriptionsRegionsPath.c_str());
        }

        auto defaultsData = getJson(testDefaultsPath);
        if (!defaultsData.empty())
        {
            auto parser = ajv::json::Parse(defaultsData);
            auto reader = parser.Reader();

            if (reader.IsObject())
            {
                from_json(reader, DefaultSettingsMap);
            }
        }
        else
        {
            SPX_TEST_TRACE_ERROR(__FILE__, "LoadFromJsonFile", __LINE__, 0, "JSON could not be loaded from %s", testDefaultsPath.c_str());
        }

        DefaultSettingsMap[INPUT_DIR] = rootPathString + DefaultSettingsMap[INPUT_DIR];
        SPX_TEST_TRACE_INFO(__FILE__, "LoadFromJsonFile", __LINE__, 0, "Setting InputDir to %s", DefaultSettingsMap[INPUT_DIR].c_str());

        auto audioUtterancesData = getJson(testAudioUtterancesPath);
        if (!audioUtterancesData.empty())
        {
            auto parser = ajv::json::Parse(audioUtterancesData);
            auto reader = parser.Reader();

            if (reader.IsObject())
            {
                from_json(reader, AudioUtterancesMap);
            }
        }
        else
        {
            SPX_TEST_TRACE_ERROR(__FILE__, "LoadFromJsonFile", __LINE__, 0, "JSON could not be loaded from %s", testAudioUtterancesPath.c_str());
        }
    }

    static void LoadFromJsonCertificate(std::string rootPathString)
    {
        SPX_TEST_TRACE_INFO(__FILE__, "LoadFromJsonCertificate", __LINE__, 0, "rootPathString is %s", rootPathString.c_str());

        std::string testCertificatePath = rootPathString + TEST_CERTIFICATE_FILE;

        auto certificateData = getJson(testCertificatePath);
        if (!certificateData.empty())
        {
            auto parser = ajv::json::Parse(certificateData);
            auto reader = parser.Reader();

            if (reader.IsObject())
            {
                from_json(reader, CertMap);
                SPX_TEST_TRACE_INFO(__FILE__, "LoadFromJsonCertificate", __LINE__, 0, "Certificate loaded successfully%s", "");
            }
            else
            {
                SPX_TEST_TRACE_ERROR(__FILE__, "LoadFromJsonCertificate", __LINE__, 0, "Invalid certificate JSON structure%s", "");
            }
        }
        else
        {
            SPX_TEST_TRACE_ERROR(__FILE__, "LoadFromJsonCertificate", __LINE__, 0, "JSON could not be loaded from %s", testCertificatePath.c_str());
        }
    }
};

inline bool checkForDiscovery(int argc, char*argv[])
{
    for (int index = 0; index < argc; index++)
    {
        if (!strcmp(argv[index], "--discovery") || !strcmp(argv[index], "--list-tests"))
        {
            return true;
        }
    }

    return false;
}

typedef std::linear_congruential_engine<uint_fast32_t, 1664525, 1013904223, UINT_FAST32_MAX> random_engine;

inline void add_signal_handlers()
{
    Debug::HookSignalHandlers();
}

#if defined(SPXTEST_PROVIDES_MAIN)
#include <test_PAL.h>

inline int parse_cli_args(Catch::Session& session, int argc, char* argv[])
{
    // Build a new parser on top of Catch's
    using namespace Catch::Clara;

    bool hasDebugFlag = false;

    auto cli
        = session.cli() // Get Catch's composite command line parser
        | Opt(SubscriptionsRegionsMap[UNIFIED_SPEECH_SUBSCRIPTION].Key, "SpeechSubscriptionKey") // bind variable to a new option, with a hint string
            ["--keySpeech"]    // the option names it will respond to
            ("The subscription key for speech")
        | Opt(SubscriptionsRegionsMap[DIALOG_SUBSCRIPTION].Key, "keyDialog")
            ["--keyDialog"]
            ("The subscription key for the Speech Channel")
        | Opt(DefaultSettingsMap[ENDPOINT], "endpoint")
            ["--endpoint"]
            ("The endpoint url to test against.")
        | Opt(SubscriptionsRegionsMap[UNIFIED_SPEECH_SUBSCRIPTION].Region, "Region")
            ["--region"]
            ("The region id to be used for subscription and authorization requests")
        | Opt(DefaultSettingsMap[CONVERSATION_TRANSCRIPTION_TEAMS_ENDPOINT], "ConversationTranscriptionTeamsEndpoint")
            ["--ConversationTranscriptionTeamsEndpoint"]
            ("The endpoint that on-line tests in intelligent meeting recognizer talks to")
        | Opt(SubscriptionsRegionsMap[CONVERSATION_TRANSCRIPTION_TEAMS_SUBSCRIPTION].Key, "ConversationTranscriptionTeamsSubscriptionKey")
            ["--keyConversationTranscriptionTeamsSubscription"]
            ("The conversation transcriber Teams endpoint key")
        | Opt(SubscriptionsRegionsMap[CONVERSATION_TRANSCRIPTION_TEAMS_SUBSCRIPTION].Region, "ConversationTranscriptionTeamsSubscriptionRegion")
            ["--regionConversationTranscriptionTeamsSubscription"]
            ("The conversation transcriber Teams endpoint region")
        | Opt(DefaultSettingsMap[INPUT_DIR], "InputDir")
            ["--inputDir"]
            ("The directory where test input files are placed")
        | Opt(SubscriptionsRegionsMap[DIALOG_SUBSCRIPTION].Region, "DialogRegion")
            ["--dialogRegionId"]
            ("The region id to be used for the Speech Channel Service")
        | Opt(DefaultSettingsMap[DIALOG_FUNCTIONAL_TEST_BOT], "DialogBotSecret")
            ["--dialogBotSecret"]
            ("Secret for the functional test bot")
        | Opt(Config::DoDiscover)
            ["--discovery"]
            ("Perform VS Test Adaptor discovery")
        | Opt(Config::MemoryLoggerExit, "always/fail/unexpected")
            ["--memory-logger-exit"]
            ("Only output the memory logger contents if process exits unexpectedly.")
        | Opt(Config::MemoryLoggerExitFile, "MemoryLoggerExitFile")
            ["--memory-logger-exit-file"]
            ("Output the memory logger contents on process exit to file specified.")
        | Opt(Config::OfflineModelPathList, "OfflineModelPathList")
            ["--offlineModelPathList"]
            ("The list of (root) paths to offline models, separated with ':'.")
        | Opt(Config::OfflineModelName, "OfflineModelName")
            ["--offlineModelName"]
            ("The name of the offline model to use.")
        | Opt(Config::OfflineModelKey, "OfflineModelKey")
            ["--offlineModelKey"]
            ("The decryption key of the offline model to use.")
        | Opt(hasDebugFlag)
            ["--debug"]
            ("Debug flag. This can be used to change certain behaviours (e.g. extend wait times)")
        ;

    // Now pass the new composite back to Catch so it uses that
    session.cli(cli);

    // Let Catch (using Clara) parse the command line
    auto ret = session.applyCommandLine(argc, argv);
    TestUtils::SetDebugFlag(hasDebugFlag);

    return ret;
}
#endif

// This is how you set up tests for carbon, always use SPXTEST_CASE_BEGIN / SPXTEST_CASE_END.
#define SPXTEST_TEMPLATE_CASE_BEGIN(...) TEMPLATE_TEST_CASE(__VA_ARGS__)
#define SPXTEST_CASE_BEGIN(...) TEST_CASE(__VA_ARGS__)
#define SPXTEST_CASE_END(...)
#define SPXTEST_SECTION(msg) SECTION(msg)
#define SPXTEST_GIVEN(msg) GIVEN(msg)
#define SPXTEST_WHEN(msg) WHEN(msg)

#ifdef __GNUC__
#define DISABLE_WARNINGS_PUSH  _Pragma("GCC diagnostic push") \
                               _Pragma("GCC diagnostic ignored \"-Wparentheses\"")
#define DISABLE_WARNINGS_POP   _Pragma("GCC diagnostic pop")
#else
#define DISABLE_WARNINGS_PUSH
#define DISABLE_WARNINGS_POP
#endif

#define SPXTEST_CHECK( ... ) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    CHECK(__VA_ARGS__); \
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_REQUIRE( ... ) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    REQUIRE(__VA_ARGS__);\
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_CHECK_FALSE( ... ) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    CHECK_FALSE(__VA_ARGS__); \
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_REQUIRE_FALSE( ... ) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    REQUIRE_FALSE(__VA_ARGS__); \
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_CHECK_THAT(arg, matcher) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    CHECK_THAT(arg, matcher); \
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_REQUIRE_THAT(arg, matcher) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    REQUIRE_THAT(arg, matcher); \
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_CAPTURE(msg) ([&](){ \
    CATCH_AUTO_LOCK(); \
    SPX_TRACE_INFO("SPXTEST_CAPTURE(%s): %s", #msg, ::Catch::Detail::stringify(msg).c_str()); \
}())

#define SPXTEST_FAIL(...) ([&](){ \
    SPXTEST_CAPTURE(__VA_ARGS__); \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    FAIL(__VA_ARGS__); \
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_CHECK_NOTHROW( ... ) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    CHECK_NOTHROW(__VA_ARGS__); \
    DISABLE_WARNINGS_POP \
}())

#define SPXTEST_REQUIRE_NOTHROW( ... ) ([&](){ \
    CATCH_AUTO_LOCK(); \
    DISABLE_WARNINGS_PUSH \
    REQUIRE_NOTHROW(__VA_ARGS__); \
    DISABLE_WARNINGS_POP \
}())

inline static void SPXTEST_REQUIRE_STRING_CONTAINS(
    const std::string fullActualString,
    const std::string expectedSubstring,
    Catch::CaseSensitive caseSensitive = Catch::CaseSensitive::No)
{
    CATCH_AUTO_LOCK();
    REQUIRE_THAT(fullActualString, Catch::Matchers::ContainsSubstring(expectedSubstring, caseSensitive));
}

#define SPXTEST_REQUIRE_THROWS_WITH_CONTAINS( expr, stringToContain ) ([&](){ \
        SPX_TRACE_INFO("SPXTEST_REQUIRE_THROWS_WITH('%s')('%s'): %s(%d):", \
            __SPX_EXPR_AS_STRING(expr), \
            stringToContain, \
            __FILE__, \
            __LINE__); \
        CATCH_AUTO_LOCK(); \
        const std::string wrappedContainString{ stringToContain }; \
        const auto matcher = Catch::Matchers::ContainsSubstring(wrappedContainString, Catch::CaseSensitive::No); \
        REQUIRE_THROWS_WITH( expr, matcher );\
    }())

inline std::string SpxGetTestTrafficType(const char* file, int line)
{
    char trafficType[1000];
    if (strrchr(file, '/') != nullptr) file = strrchr(file, '/') + 1;
    if (strrchr(file, '\\') != nullptr) file = strrchr(file, '\\') + 1;

    auto buildid = PAL::SpxGetEnv("BUILD_BUILDID").GetOr("dev");

    snprintf(trafficType, sizeof(trafficType), "%s(%d)%%20bld(%s)", file, line, buildid.c_str());
    return trafficType;
}

class Awaiter
{
public:
    inline void Signal()
    {
        m_cv.notify_all();
    }

    template<typename R, typename P>
    bool WaitFor(const std::chrono::duration<R, P>& duration)
    {
        std::unique_lock<std::mutex> lk{ m_mutex };
        return m_cv.wait_for(lk, duration) == std::cv_status::no_timeout;
    }

    template<typename R, typename P, typename F>
    bool WaitFor(const std::chrono::duration<R, P>& duration, F predicate)
    {
        std::unique_lock<std::mutex> lk{ m_mutex };
        if (predicate())
        {
            return true;
        }
        return m_cv.wait_for(lk, duration, predicate);
    }

private:
    std::mutex m_mutex{};
    std::condition_variable m_cv{};
};

/// <summary>
/// RAII guard that joins a worker std::thread when the guard goes out of scope,
/// including when the scope is left via a thrown exception (e.g. a failing
/// SPXTEST_REQUIRE / Catch2 REQUIRE). Destroying a still-joinable std::thread
/// calls std::terminate() -> abort() (SIGABRT), which would otherwise turn any
/// assertion failure between thread creation and a manual join into a process
/// abort that kills the whole test run instead of failing a single test.
///
/// Usage:
///     auto worker = std::thread(SomeWork, args...);
///     ThreadJoinGuard workerGuard(worker);
///     // ... SPXTEST_REQUIRE(...) etc. may throw here; worker is still joined ...
///     // No manual join needed; the guard joins on normal or exceptional exit.
/// </summary>
class ThreadJoinGuard
{
public:
    explicit ThreadJoinGuard(std::thread& thread) : m_thread(thread) {}

    ~ThreadJoinGuard()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    ThreadJoinGuard(const ThreadJoinGuard&) = delete;
    ThreadJoinGuard& operator=(const ThreadJoinGuard&) = delete;
    ThreadJoinGuard(ThreadJoinGuard&&) = delete;
    ThreadJoinGuard& operator=(ThreadJoinGuard&&) = delete;

private:
    std::thread& m_thread;
};

template<typename TRet>
TRet WaitForFuture(std::future<TRet>& future, std::chrono::milliseconds maxWaitTime = 2000ms, const std::string& description = "Waiting for future")
{
    auto result = future.wait_for(maxWaitTime);
    switch (result)
    {
    case std::future_status::ready:
        return future.get();

    case std::future_status::timeout:
        throw Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack("Timeout: " + description);

    default:
    case std::future_status::deferred:
        throw Microsoft::CognitiveServices::Speech::Impl::ExceptionWithCallStack("Unexpected status: " + description);
    }
}

/// <summary>
/// Wait until the specified predicate returns true, or we time out. The predicate will always be evaluated
/// immediately at least once
/// </summary>
/// <typeparam name="TPredicate">The type of the predicate that returns true when we are done</typeparam>
/// <param name="maxWaitTime">The maximum amount of time to wait</param>
/// <param name="pollPeriod">How often to check the predicate</param>
/// <returns>True if the predicate became true, false if we timed out</returns>
template<typename TPredicate>
bool WaitUntil(TPredicate pred, const std::chrono::milliseconds& maxWaitTime = 2000ms, const std::chrono::milliseconds& pollPeriod = 250ms)
{
    if (maxWaitTime < 0ms)
    {
        return false;
    }

    auto stopAt = std::chrono::steady_clock::now() + maxWaitTime;
    while(true)
    {
        bool complete = pred();
        if (complete)
        {
            return true;
        }

        if (std::chrono::steady_clock::now() > stopAt)
        {
            return false;
        }

        std::this_thread::sleep_for(pollPeriod);
    }

    // should never get here
    return false;
}

inline std::vector<uint8_t> GenerateRandomData(size_t dataSize)
{
    auto frame = std::vector<uint8_t>(dataSize);
    for (size_t i = 0; i < frame.size(); i++)
    {
        frame[i] = (uint8_t)rand();
    }

    return frame;
}

template<typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
std::vector<T> Sequence(size_t size)
{
    std::vector<T> v(size);
    std::iota(v.begin(), v.end(), 0);
    return v;
}

// The following allow macro expansion in test cases to print the friendly names of enums instead of the fallback int
// values from static_cast. This was generated (regex'd) from speechapi_cxx_enums.h.
// Note: we *could* do this in product code somewhere (e.g. via an operator<< overload) but it's not clear we want to
// foist serialization into customer environments.

CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::PropertyId,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_Key,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_Endpoint,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_Region,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceAuthorization_Token,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceAuthorization_Type,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_EndpointId,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_Host,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_ProxyHostName,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_ProxyPort,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_ProxyUserName,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_ProxyPassword,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_Url,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_TranslationToLanguages,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_TranslationVoice,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_TranslationFeatures,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_RecoMode,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_RecoLanguage,
    Microsoft::CognitiveServices::Speech::PropertyId::Speech_SessionId,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_UserDefinedQueryParameters,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_SynthLanguage,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_SynthVoice,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_SynthOutputFormat,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_SynthEnableCompressedAudioTransmission,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_VoicesListEndpoint,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_InitialSilenceTimeoutMs,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_EndSilenceTimeoutMs,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_EnableAudioLogging,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_LanguageIdMode,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_TranslationCategoryId,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceConnection_AutoDetectSourceLanguageResult,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_RequestDetailedResultTrueFalse,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_RequestProfanityFilterTrueFalse,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_ProfanityOption,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_PostProcessingOption,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_RequestWordLevelTimestamps,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_StablePartialResultThreshold,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_OutputFormatOption,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_RequestSnr,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_TranslationRequestStablePartialResult,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_JsonResult,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_JsonErrorDetails,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_RecognitionLatencyMs,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_SynthesisFirstByteLatencyMs,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_SynthesisFinishLatencyMs,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_SynthesisUnderrunTimeMs,
    Microsoft::CognitiveServices::Speech::PropertyId::SpeechServiceResponse_SynthesisBackend,
    Microsoft::CognitiveServices::Speech::PropertyId::CancellationDetails_Reason,
    Microsoft::CognitiveServices::Speech::PropertyId::CancellationDetails_ReasonText,
    Microsoft::CognitiveServices::Speech::PropertyId::CancellationDetails_ReasonDetailedText,
    Microsoft::CognitiveServices::Speech::PropertyId::AudioConfig_DeviceNameForCapture,
    Microsoft::CognitiveServices::Speech::PropertyId::AudioConfig_NumberOfChannelsForCapture,
    Microsoft::CognitiveServices::Speech::PropertyId::AudioConfig_SampleRateForCapture,
    Microsoft::CognitiveServices::Speech::PropertyId::AudioConfig_BitsPerSampleForCapture,
    Microsoft::CognitiveServices::Speech::PropertyId::AudioConfig_AudioSource,
    Microsoft::CognitiveServices::Speech::PropertyId::AudioConfig_DeviceNameForRender,
    Microsoft::CognitiveServices::Speech::PropertyId::AudioConfig_PlaybackBufferLengthInMs,
    Microsoft::CognitiveServices::Speech::PropertyId::Speech_LogFilename,
    Microsoft::CognitiveServices::Speech::PropertyId::Speech_SegmentationSilenceTimeoutMs,
    Microsoft::CognitiveServices::Speech::PropertyId::Speech_SegmentationMaximumTimeMs,
    Microsoft::CognitiveServices::Speech::PropertyId::Speech_SegmentationStrategy,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_ApplicationId,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_DialogType,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_Initial_Silence_Timeout,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_From_Id,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_Conversation_Id,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_Custom_Voice_Deployment_Ids,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_Speech_Activity_Template,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_Request_Bot_Status_Messages,
    Microsoft::CognitiveServices::Speech::PropertyId::Conversation_Connection_Id,
    Microsoft::CognitiveServices::Speech::PropertyId::DataBuffer_TimeStamp,
    Microsoft::CognitiveServices::Speech::PropertyId::DataBuffer_UserId,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_ReferenceText,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_GradingSystem,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_Granularity,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_EnableMiscue,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_PhonemeAlphabet,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_NBestPhonemeCount,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_Json,
    Microsoft::CognitiveServices::Speech::PropertyId::PronunciationAssessment_Params)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::OutputFormat,
    Microsoft::CognitiveServices::Speech::OutputFormat::Simple,
    Microsoft::CognitiveServices::Speech::OutputFormat::Detailed)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::ProfanityOption,
    Microsoft::CognitiveServices::Speech::ProfanityOption::Masked,
    Microsoft::CognitiveServices::Speech::ProfanityOption::Removed,
    Microsoft::CognitiveServices::Speech::ProfanityOption::Raw)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::ResultReason,
    Microsoft::CognitiveServices::Speech::ResultReason::NoMatch,
    Microsoft::CognitiveServices::Speech::ResultReason::Canceled,
    Microsoft::CognitiveServices::Speech::ResultReason::RecognizingSpeech,
    Microsoft::CognitiveServices::Speech::ResultReason::RecognizedSpeech,
    Microsoft::CognitiveServices::Speech::ResultReason::TranslatingSpeech,
    Microsoft::CognitiveServices::Speech::ResultReason::TranslatedSpeech,
    Microsoft::CognitiveServices::Speech::ResultReason::SynthesizingAudio,
    Microsoft::CognitiveServices::Speech::ResultReason::SynthesizingAudioCompleted,
    Microsoft::CognitiveServices::Speech::ResultReason::RecognizingKeyword,
    Microsoft::CognitiveServices::Speech::ResultReason::RecognizedKeyword,
    Microsoft::CognitiveServices::Speech::ResultReason::SynthesizingAudioStarted,
    Microsoft::CognitiveServices::Speech::ResultReason::TranslatingParticipantSpeech,
    Microsoft::CognitiveServices::Speech::ResultReason::TranslatedParticipantSpeech,
    Microsoft::CognitiveServices::Speech::ResultReason::TranslatedInstantMessage,
    Microsoft::CognitiveServices::Speech::ResultReason::TranslatedParticipantInstantMessage,
    Microsoft::CognitiveServices::Speech::ResultReason::VoicesListRetrieved)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::CancellationReason,
    Microsoft::CognitiveServices::Speech::CancellationReason::Error,
    Microsoft::CognitiveServices::Speech::CancellationReason::EndOfStream,
    Microsoft::CognitiveServices::Speech::CancellationReason::CancelledByUser)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::CancellationErrorCode,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::NoError,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::AuthenticationFailure,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::BadRequest,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::TooManyRequests,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::Forbidden,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::ConnectionFailure,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::ServiceTimeout,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::ServiceError,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::ServiceUnavailable,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::RuntimeError,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::ServiceRedirectTemporary,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::ServiceRedirectPermanent,
    Microsoft::CognitiveServices::Speech::CancellationErrorCode::EmbeddedModelError)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::NoMatchReason,
    Microsoft::CognitiveServices::Speech::NoMatchReason::NotRecognized,
    Microsoft::CognitiveServices::Speech::NoMatchReason::InitialSilenceTimeout,
    Microsoft::CognitiveServices::Speech::NoMatchReason::InitialBabbleTimeout,
    Microsoft::CognitiveServices::Speech::NoMatchReason::KeywordNotRecognized,
    Microsoft::CognitiveServices::Speech::NoMatchReason::EndSilenceTimeout)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::ActivityJSONType,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::Null,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::Object,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::Array,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::String,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::Double,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::UInt,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::Int,
    Microsoft::CognitiveServices::Speech::ActivityJSONType::Boolean)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw8Khz8BitMonoMULaw,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff16Khz16KbpsMonoSiren,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio16Khz16KbpsMonoSiren,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio16Khz32KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio16Khz128KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio16Khz64KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio24Khz48KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio24Khz96KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio24Khz160KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw16Khz16BitMonoTrueSilk,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff16Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff8Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff24Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff8Khz8BitMonoMULaw,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw16Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw24Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw8Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Ogg16Khz16BitMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Ogg24Khz16BitMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw48Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff48Khz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio48Khz96KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio48Khz192KBitRateMonoMp3,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Ogg48Khz16BitMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Webm16Khz16BitMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Webm24Khz16BitMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw24Khz16BitMonoTrueSilk,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw8Khz8BitMonoALaw,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff8Khz8BitMonoALaw,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Webm24Khz16Bit24KbpsMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio16Khz16Bit32KbpsMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio24Khz16Bit48KbpsMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Audio24Khz16Bit24KbpsMonoOpus,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw22050Hz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff22050Hz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Raw44100Hz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::Riff44100Hz16BitMonoPcm,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::AmrWb16000Hz,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisOutputFormat::G72216Khz64Kbps)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::StreamStatus,
    Microsoft::CognitiveServices::Speech::StreamStatus::Unknown,
    Microsoft::CognitiveServices::Speech::StreamStatus::NoData,
    Microsoft::CognitiveServices::Speech::StreamStatus::PartialData,
    Microsoft::CognitiveServices::Speech::StreamStatus::AllData,
    Microsoft::CognitiveServices::Speech::StreamStatus::Canceled)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::ServicePropertyChannel,
    Microsoft::CognitiveServices::Speech::ServicePropertyChannel::UriQueryParameter,
    Microsoft::CognitiveServices::Speech::ServicePropertyChannel::HttpHeader)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::RecognitionFactorScope,
    Microsoft::CognitiveServices::Speech::RecognitionFactorScope::PartialPhrase)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::PronunciationAssessmentGradingSystem,
    Microsoft::CognitiveServices::Speech::PronunciationAssessmentGradingSystem::FivePoint,
    Microsoft::CognitiveServices::Speech::PronunciationAssessmentGradingSystem::HundredMark)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::PronunciationAssessmentGranularity,
    Microsoft::CognitiveServices::Speech::PronunciationAssessmentGranularity::Phoneme,
    Microsoft::CognitiveServices::Speech::PronunciationAssessmentGranularity::Word,
    Microsoft::CognitiveServices::Speech::PronunciationAssessmentGranularity::FullText)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::SynthesisVoiceType,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceType::OnlineNeural,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceType::OnlineStandard,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceType::OfflineNeural,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceType::OfflineStandard)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::SynthesisVoiceGender,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceGender::Unknown,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceGender::Female,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceGender::Male,
    Microsoft::CognitiveServices::Speech::SynthesisVoiceGender::Neutral)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType::Word,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType::Punctuation,
    Microsoft::CognitiveServices::Speech::SpeechSynthesisBoundaryType::Sentence)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::Unknown,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::Normal,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::EndpointUnavailable,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::ProtocolError,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::CannotAcceptDataType,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::InvalidPayloadData,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::PolicyViolation,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::MessageTooBig,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::UnexpectedCondition,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::InternalServerError,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::ResourceExhausted,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::UnsupportedLocale,
    Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason::RequestThrottled)
CATCH_REGISTER_ENUM(
    Microsoft::CognitiveServices::Speech::Impl::UriScheme,
    Microsoft::CognitiveServices::Speech::Impl::UriScheme::FILE,
    Microsoft::CognitiveServices::Speech::Impl::UriScheme::HTTP,
    Microsoft::CognitiveServices::Speech::Impl::UriScheme::HTTPS,
    Microsoft::CognitiveServices::Speech::Impl::UriScheme::WS,
    Microsoft::CognitiveServices::Speech::Impl::UriScheme::WSS
)

inline std::string GetLogFileName()
{
    // This will return something like "2022-05-11T16:49:58.1012092Z"
    std::string utcTimeStamp = PAL::GetUtcTimestamp();

    // Since `:` is not a valid character in a Windows file name, replace with `-`
    std::replace(utcTimeStamp.begin(), utcTimeStamp.end(), ':', '-');

    // Make sure environment variable %TEMP% is defined.
    // On Windows this will return something like:
    // C:\Users\<<your-user-name>>\AppData\Local\Temp\sdk-log-2022-05-11T16-49-58.1012092Z.txt
    return PAL::SpxGetEnv("TEMP").GetOr("") + "\\sdk-log-" + utcTimeStamp + ".txt";
}
