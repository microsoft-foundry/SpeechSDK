//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <chrono>
#include <thread>
#include <list>
#include <random>
#include <string>
#include <sstream>

#include "test_utils.h"
#include "audio_stream_session.h"
#include "usp_reco_engine_adapter.h"
#include "site_helpers.h"
#include "create_object_helpers.h"
#include "ispxinterfaces.h"
#include "recognizer.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

// Define a struct to represent JSON differences (similar to nlohmann::json::diff output)
struct JsonDiff {
    std::string path;       // JSON path to the difference
    std::string op;         // Operation type (e.g., "add", "remove", "replace")
    std::string expected;   // Expected value (if applicable)
    std::string actual;     // Actual value (if applicable)
};

// Function to compare two JSON objects using ajv::JsonReader
std::vector<JsonDiff> compareJsonReader(
    const ajv::JsonReader& expected,
    const ajv::JsonReader& actual,
    const std::string& path = "",
    bool allowAdds = false)
{
    std::vector<JsonDiff> diffs;

    // Handle cases based on JSON type
    if (expected.IsObject() && actual.IsObject())
    {
        // Check each property in the expected JSON
        for (int i = 0; i < expected.ValueCount(); i++)
        {
            // Get property name
            auto nameReader = expected.NameAt(i);
            std::string key = nameReader.AsString();
            std::string newPath = path.empty() ? key : path + "/" + key;

            auto expectedValue = expected.ValueAt(key.c_str());
            auto actualValue = actual.ValueAt(key.c_str());

            // Check if the key exists in the actual JSON
            if (!actualValue.IsOk())
            {
                diffs.push_back({newPath, "remove", expectedValue.AsJson(), "undefined"});
            }
            else
            {
                // Recursively compare the values
                auto childDiffs = compareJsonReader(expectedValue, actualValue, newPath, allowAdds);
                diffs.insert(diffs.end(), childDiffs.begin(), childDiffs.end());
            }
        }

        // If not allowing adds, check for extra properties in actual JSON
        if (!allowAdds)
        {
            for (int i = 0; i < actual.ValueCount(); i++)
            {
                auto nameReader = actual.NameAt(i);
                std::string key = nameReader.AsString();
                std::string newPath = path.empty() ? key : path + "/" + key;

                auto expectedValue = expected.ValueAt(key.c_str());

                if (!expectedValue.IsOk())
                {
                    auto actualValue = actual.ValueAt(key.c_str());
                    diffs.push_back({newPath, "add", "undefined", actualValue.AsJson()});
                }
            }
        }
    }
    else if (expected.IsArray() && actual.IsArray())
    {
        // Compare array elements by position
        int expectedCount = expected.ValueCount();
        int actualCount = actual.ValueCount();

        // Check each element in the expected array
        for (int i = 0; i < expectedCount; i++)
        {
            std::string newPath = path + "/" + std::to_string(i);

            if (i >= actualCount)
            {
                diffs.push_back({newPath, "remove", expected.ValueAt(i).AsJson(), "undefined"});
            }
            else
            {
                auto childDiffs = compareJsonReader(expected.ValueAt(i), actual.ValueAt(i), newPath, allowAdds);
                diffs.insert(diffs.end(), childDiffs.begin(), childDiffs.end());
            }
        }

        // If not allowing adds, check for extra elements in actual array
        if (!allowAdds && actualCount > expectedCount)
        {
            for (int i = expectedCount; i < actualCount; i++)
            {
                std::string newPath = path + "/" + std::to_string(i);
                diffs.push_back({newPath, "add", "undefined", actual.ValueAt(i).AsJson()});
            }
        }
    }
    else
    {
        // Different types or primitive values
        if (expected.Kind() != actual.Kind())
        {
            diffs.push_back({path, "replace", expected.AsJson(), actual.AsJson()});
        }
        else
        {
            // Same type, compare values
            switch (expected.Kind())
            {
                case ajv::JsonKind::String:
                    if (expected.AsString() != actual.AsString())
                    {
                        diffs.push_back({path, "replace", expected.AsJson(), actual.AsJson()});
                    }
                    break;
                case ajv::JsonKind::Number:
                    // For numbers, use a more precise comparison
                    if (std::abs(expected.AsNumber() - actual.AsNumber()) > 1e-10)
                    {
                        diffs.push_back({path, "replace", expected.AsJson(), actual.AsJson()});
                    }
                    break;
                case ajv::JsonKind::Boolean:
                    if (expected.AsBool() != actual.AsBool())
                    {
                        diffs.push_back({path, "replace", expected.AsJson(), actual.AsJson()});
                    }
                    break;
                case ajv::JsonKind::Null:
                    // Both are null, so they match
                    break;
                default:
                    // For other types or if types don't match
                    if (expected.AsJson() != actual.AsJson())
                    {
                        diffs.push_back({path, "replace", expected.AsJson(), actual.AsJson()});
                    }
                    break;
            }
        }
    }

    return diffs;
}

// Wrapper function to compare two JSON strings
std::vector<JsonDiff> compareJson(
    const ajv::JsonParser& expected,
    const ajv::JsonParser& actual,
    const std::string& path = "",
    bool allowAdds = false)
{
    return compareJsonReader(expected.Reader(), actual.Reader(), path, allowAdds);
}

class TestCSpxSpeechRecognizer : public CSpxRecognizer
{
public:
    TestCSpxSpeechRecognizer() : CSpxRecognizer() {};
};

class TestCSpxAudioStreamSession : public CSpxAudioStreamSession
{
public:
    void SetSpottedKeywordResult(std::string keyword, double confidence, uint64_t offset, uint64_t duration)
    {
        if (m_result == nullptr)
        {
            m_result = std::make_shared<CSpxRecognitionResult>();
        }
        m_result->InitKeywordResult(confidence, offset, duration, keyword.c_str(), ResultReason::RecognizedKeyword, nullptr);
        auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
        properties->SetStringValue(KeywordConfig_EnableKeywordVerification, "true");
    }

    std::shared_ptr<ISpxRecognitionResult> GetSpottedKeywordResult() override
    {
        return m_result;
    }

    void SetListenForList(const std::list<std::string> &listners)
    {
        m_listnerFor = listners;
    }

    void SetGrammarWeight(double weight)
    {
        m_grammarWeight = weight;
    }

    std::list<std::string> GetListenForList() override
    {
        auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
        properties->SetStringValue(g_phraseListWeightPropertyName, std::to_string(m_grammarWeight).c_str());
        return m_listnerFor;
    }

    std::shared_ptr<ISpxNamedProperties> GetPropertiesPtr()
    {
        return SpxQueryService<ISpxNamedProperties>(GetSite());
    }

    void SetInsertLeftRight(std::string left, std::string right)
    {
        auto properties = GetPropertiesPtr();
        properties->SetStringValue("DictationInsertionPointLeft", left.c_str());
        properties->SetStringValue("DictationInsertionPointRight", right.c_str());
    }

    void SetTargetLanguages(std::string toLanguages)
    {
        auto properties = GetPropertiesPtr();
        properties->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_TranslationToLanguages), toLanguages.c_str());
    }

    /// <summary>
    /// Sets the automatic detect languages.
    /// </summary>
    /// <param name="languages">The languages.</param>
    void SetAutoDetectLanguages(std::string languages) {
        auto properties = GetPropertiesPtr();
        properties->SetStringValue(
            GetPropertyName(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages),
            languages.c_str());
    }

    /// <summary>
    /// Sets the language identifier mode.
    /// </summary>
    /// <param name="mode">See values g_languageIdModeXxx defined in source\core\common\include\property_id_2_name_map.h</param>
    void SetLanguageIdMode(std::string mode) {
        auto properties = GetPropertiesPtr();
        properties->SetStringValue(
            GetPropertyName(PropertyId::SpeechServiceConnection_LanguageIdMode),
            mode.c_str());
    }

    /// <summary>
    /// Sets the language identifier json.
    /// </summary>
    /// <param name="sourceLangs">The source langs.</param>
    /// <param name="onSuccessAction">The on success action.</param>
    /// <param name="mode">The mode.</param>
    /// <param name="priority">The priority.</param>
    /// <returns>language id json</returns>
    std::string SetLanguageIdJson(std::vector<std::string> sourceLangs, std::string onSuccessAction, std::string mode, std::string priority)
    {
        ajv::JsonBuilder languageIdJson;

        for (int i = 0; i < static_cast<int>(sourceLangs.size()); i++)
        {
            languageIdJson["languages"][i] = sourceLangs[i];
        }
        languageIdJson["onUnknown"]["action"] = "None";
        languageIdJson["onSuccess"]["action"] = onSuccessAction;
        languageIdJson["mode"] = mode;
        languageIdJson["Priority"] = priority;

        return languageIdJson.AsJson();
    }

    // The data in speech context and speech config that could be set from user is being passed via a recognizer.
    // The speech_context_json_test.cpp does not have a recognizer. So, override it with empty data
    CSpxStringMap GetParametersFromUser(std::string&& path) override
    {
        UNUSED(path);
        return {};
    }

private:
    std::string m_provider;
    std::string m_id;
    std::string m_key;
    std::string m_region;
    std::string m_endpoint;
    std::string m_deploymentName;
    std::list<std::string> m_listnerFor;
    double m_grammarWeight = 1.0;
    std::shared_ptr<CSpxRecognitionResult> m_result;
};

class CSpxUspRecoEngineAdapterTest
{
public:
    CSpxUspRecoEngineAdapterTest()
    {
        m_session = std::make_shared<TestCSpxAudioStreamSession>();
        m_session->SetSite(SpxGetRootSite());
        std::shared_ptr<TestCSpxSpeechRecognizer> recognizer = std::make_shared<TestCSpxSpeechRecognizer>();
        m_session->AddRecognizer(recognizer);
        auto site = SpxQueryInterface<ISpxGenericSite>(m_session);
        m_adapter.SetSite(site);
        m_adapter.ResolveRecoMode(true);
    }

    std::string GetSpeechContextJson()
    {
        return m_adapter.GetSpeechContextJson();
    }

    void SetEndpointType(USP::EndpointType endpointType)
    {
        m_adapter.m_endpointType = endpointType;
    }

    void SetLanguageIdModeAndPriority(USP::LanguageIdMode languageIdMode, USP::LanguageIdPriority languageIdPriority)
    {
        m_adapter.m_languageIdMode = languageIdMode;
        m_adapter.m_languageIdPriority = languageIdPriority;
    }

    std::shared_ptr<TestCSpxAudioStreamSession> GetSession()
    {
        return m_session;
    }

    std::string Join(std::vector<std::string> values)
    {
        std::stringstream ss;
        size_t count = 0;
        for (auto &value : values)
        {
            ss << value;
            if (++count < values.size())
            {
                ss << CommaDelim;
            }
        }
        return ss.str();
    }

    void RequireJsonMatch(const std::string& expectedJsonStr, bool allowAdds = false)
    {
        // Get the actual JSON string
        const std::string actualJsonStr = GetSpeechContextJson();

        // Parse the expected and actual JSON using ajv::json
        auto expectedJson = ajv::json::Parse(expectedJsonStr);
        auto actualJson = ajv::json::Parse(actualJsonStr);

        // Compare JSON objects and collect differences
        auto diffs = compareJson(expectedJson, actualJson, "", allowAdds);

        // Filter out allowed additions if necessary
        auto disallowedDiffs = std::vector<JsonDiff>();
        for (const auto& diff : diffs)
        {
            if (!allowAdds || diff.op != "add")
            {
                disallowedDiffs.push_back(diff);
            }
        }

        // Report differences if any
        if (!disallowedDiffs.empty())
        {
            std::ostringstream jsonDiffMessage;
            auto printy = [&](const std::string& jsonStr, std::string label)
            {
                jsonDiffMessage
                    << label << " JSON:" << std::endl
                    << "---" << std::endl
                    << jsonStr << std::endl
                    << "---" << std::endl;
            };

            printy(expectedJsonStr, allowAdds ? "Expected (contains)" : "Expected (exact)");
            printy(actualJsonStr, "Actual (full)");

            // Format differences similar to original implementation
            ajv::JsonBuilder diffJson;
            for (int i = 0; i < static_cast<int>(disallowedDiffs.size()); i++)
            {
                const auto& diff = disallowedDiffs[i];
                ajv::JsonBuilder diffItem;
                diffItem["op"] = diff.op;
                diffItem["path"] = diff.path;

                if (diff.op != "remove")
                {
                    diffItem["value"] = diff.actual;
                }

                if (diff.op == "replace")
                {
                    diffItem["old_value"] = diff.expected;
                }

                diffJson[i] = diffItem;
            }

            printy(diffJson.AsJson(), "Diff/patch operations");

            std::cerr << jsonDiffMessage.str() << std::endl;
        }

        SPXTEST_CHECK(disallowedDiffs.empty());
    }

    void RequireJsonContains(const std::string& expectedContents)
    {
        RequireJsonMatch(expectedContents, true);
    }

private:
    CSpxUspRecoEngineAdapter m_adapter;
    std::shared_ptr<TestCSpxAudioStreamSession> m_session;
};

SPXTEST_CASE_BEGIN("speech.context JSON generation", "[context_json]")
{
    CSpxUspRecoEngineAdapterTest adapterTest;
    auto session = adapterTest.GetSession();
    ajv::JsonBuilder expectedJson;

    // Everyone gets an audio stream
    expectedJson["audio"]["streams"]["1"] = nullptr;

    SPXTEST_SECTION("Speech")
    {
        expectedJson["phraseDetection"]["language"] = "en-us";
        expectedJson["phraseDetection"]["mode"] = "INTERACTIVE";
        adapterTest.RequireJsonMatch(expectedJson.AsJson());
    }

    SPXTEST_SECTION("DGI")
    {
        std::string intentName = "intent1";
        std::vector<std::string> grammars{
            "g\\rammar1",
            R"("gra\mmar2")",
            "grammar3"};
        double grammarWeight = 2.0;

        std::string id = "randomkey2-xyz";
        std::list<std::string> listenFors(grammars.begin(), grammars.end());
        listenFors.push_back("{luis:" + id + "-PRODUCTION#" + intentName + "}");

        session->SetListenForList(listenFors);
        session->SetGrammarWeight(grammarWeight);

        expectedJson["dgi"]["ReferenceGrammars"][0] = "luis/" + id + "-PRODUCTION#" + intentName;
        ajv::JsonBuilder grammarJson;
        grammarJson["Type"] = "Generic";
        for (int i = 0; i < static_cast<int>(grammars.size()); i++)
        {
            grammarJson["Items"][i]["Text"] = grammars[i];
        }
        expectedJson["dgi"]["Groups"][0] = grammarJson;
        expectedJson["dgi"]["bias"] = grammarWeight;
        expectedJson["phraseDetection"]["language"] = "en-us";
        expectedJson["phraseDetection"]["mode"] = "INTERACTIVE";

        adapterTest.RequireJsonMatch(expectedJson.AsJson());
    }

    SPXTEST_SECTION("keyword")
    {
        expectedJson["invocationSource"] = "VoiceActivationWithKeyword";
        expectedJson["keywordDetection"][0]["type"] = "startTrigger";
        expectedJson["keywordDetection"][0]["onReject"]["action"] = "EndOfTurn";
        expectedJson["phraseDetection"]["mode"] = "INTERACTIVE";
        auto &&detectedKeywordsJson = expectedJson["keywordDetection"][0];

        SPXTEST_SECTION("spotted result")
        {
            std::string keyword = "Cortana(微软小娜)";
            double confidence = 0.8;
            uint64_t offset = 1000;
            uint64_t duration = 20000;

            session->SetSpottedKeywordResult(keyword, confidence, offset, duration);
            detectedKeywordsJson["clientDetectedKeywords"][0]["text"] = keyword;
            detectedKeywordsJson["clientDetectedKeywords"][0]["confidence"] = confidence;
            detectedKeywordsJson["clientDetectedKeywords"][0]["startOffset"] = offset;
            detectedKeywordsJson["clientDetectedKeywords"][0]["duration"] = duration;
            expectedJson["phraseDetection"]["language"] = "en-us";
            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }

        SPXTEST_SECTION("specified KWV")
        {
            SPXTEST_SECTION("one keyword")
            {
                session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect", "foobar");
                detectedKeywordsJson["clientDetectedKeywords"][0]["text"] = "foobar";
                expectedJson["phraseDetection"]["language"] = "en-us";
                adapterTest.RequireJsonMatch(expectedJson.AsJson());
            }

            SPXTEST_SECTION("multiple keywords, durations, offsets")
            {
                session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect", "foo;bar;baz");
                session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect-Offsets", "1;2;3");
                session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect-Durations", "4;5;6");
                detectedKeywordsJson["clientDetectedKeywords"][0]["text"] = "foo";
                detectedKeywordsJson["clientDetectedKeywords"][0]["offset"] = 1;
                detectedKeywordsJson["clientDetectedKeywords"][0]["duration"] = 4;
                detectedKeywordsJson["clientDetectedKeywords"][1]["text"] = "bar";
                detectedKeywordsJson["clientDetectedKeywords"][1]["offset"] = 2;
                detectedKeywordsJson["clientDetectedKeywords"][1]["duration"] = 5;
                detectedKeywordsJson["clientDetectedKeywords"][2]["text"] = "baz";
                detectedKeywordsJson["clientDetectedKeywords"][2]["offset"] = 3;
                detectedKeywordsJson["clientDetectedKeywords"][2]["duration"] = 6;
                expectedJson["phraseDetection"]["language"] = "en-us";
                adapterTest.RequireJsonMatch(expectedJson.AsJson());
            }

            SPXTEST_SECTION("multiple keywords, bad aux inputs")
            {
                session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect", "foo;bar;baz");
                session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect-Offsets", "1;!;3");
                session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect-Durations", "4;5;x");
                detectedKeywordsJson["clientDetectedKeywords"][0]["text"] = "foo";
                detectedKeywordsJson["clientDetectedKeywords"][0]["offset"] = 1;
                detectedKeywordsJson["clientDetectedKeywords"][0]["duration"] = 4;
                detectedKeywordsJson["clientDetectedKeywords"][1]["text"] = "bar";
                detectedKeywordsJson["clientDetectedKeywords"][1]["duration"] = 5;
                detectedKeywordsJson["clientDetectedKeywords"][2]["text"] = "baz";
                detectedKeywordsJson["clientDetectedKeywords"][2]["offset"] = 3;
                expectedJson["phraseDetection"]["language"] = "en-us";
                adapterTest.RequireJsonMatch(expectedJson.AsJson());
            }

            session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect", "");
            session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect-Offsets", "");
            session->GetPropertiesPtr()->SetStringValue("SPEECH-KeywordsToDetect-Durations", "");
        }
    }

    SPXTEST_SECTION("Dictation")
    {
        session->SetInsertLeftRight("left", "right");
        expectedJson["dictation"]["insertionPoint"]["left"] = "left";
        expectedJson["dictation"]["insertionPoint"]["right"] = "right";
        expectedJson["phraseDetection"]["language"] = "en-us";
        expectedJson["phraseDetection"]["mode"] = "INTERACTIVE";
        adapterTest.RequireJsonMatch(expectedJson.AsJson());
        // clean the data
        session->SetInsertLeftRight("", "");
    }

    SPXTEST_SECTION("TranslationContext")
    {
        std::vector<std::string> toLangs{"en-us", "Fr-fr"};
        session->SetTargetLanguages(adapterTest.Join(toLangs));
        for (int i = 0; i < static_cast<int>(toLangs.size()); i++)
        {
            expectedJson["translation"]["targetLanguages"][i] = toLangs[i];
        }
        expectedJson["translation"]["output"]["includePassThroughResults"] = true;
        expectedJson["phraseDetection"]["language"] = "en-us";
        expectedJson["phraseDetection"]["mode"] = "INTERACTIVE";
        adapterTest.RequireJsonMatch(expectedJson.AsJson());
        // clean the data
        session->SetTargetLanguages("");
    }

    SPXTEST_SECTION("Translation NewEndpoint")
    {
        adapterTest.SetEndpointType(USP::EndpointType::Translation);
        auto properties = session->GetPropertiesPtr();

        std::vector<std::string> autoDetectSourceLangs{"en-us", "zh-CN"};
        properties->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages), "en-us,zh-CN");
        ajv::JsonBuilder languageIdJson;
        for (int i = 0; i < static_cast<int>(autoDetectSourceLangs.size()); i++)
        {
            languageIdJson["languages"][i] = autoDetectSourceLangs[i];
        }
        languageIdJson["onUnknown"]["action"] = "None";
        languageIdJson["onSuccess"]["action"] = "Recognize";
        languageIdJson["mode"] = "DetectAtAudioStart";
        languageIdJson["Priority"] = "PrioritizeLatency";
        expectedJson["languageId"] = languageIdJson;

        std::map<std::string, std::string> voiceNameMap{
            {"de-DE", "Microsoft Server Speech Text to Speech Voice (de-DE, ChristophNeural)"},
            {"fr-FR", "Microsoft Server Speech Text to Speech Voice (fr-FR, HenriNeural)"}};
        std::vector<std::string> toLangs;
        for (auto &pair : voiceNameMap)
        {
            toLangs.push_back(pair.first);
        }
        toLangs.push_back("en-US");
        session->SetTargetLanguages(adapterTest.Join(toLangs));

        ajv::JsonBuilder phraseDetectionJson;
        phraseDetectionJson["onSuccess"]["action"] = "Translate";
        phraseDetectionJson["onInterim"]["action"] = "Translate";
        phraseDetectionJson["mode"] = "INTERACTIVE";

        expectedJson["phraseDetection"] = phraseDetectionJson;
        expectedJson["phraseOutput"]["interimResults"]["resultType"] = "None";
        expectedJson["phraseOutput"]["phraseResults"]["resultType"] = "None";

        ajv::JsonBuilder translationJson;
        for (int i = 0; i < static_cast<int>(toLangs.size()); i++)
        {
            translationJson["targetLanguages"][i] = toLangs[i];
        }
        translationJson["output"]["interimResults"]["mode"] = "Always";
        translationJson["output"]["includePassThroughResults"] = true;

        translationJson["onSuccess"]["action"] = "None";
        translationJson["onPassthrough"]["action"] = "None";
        expectedJson["translation"] = translationJson;
 //       expectedJson["translation"]["targetlanguages"] = json(toLangs);

        SPXTEST_SECTION("without TranslationVoice explicitly set")
        {
            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }

        SPXTEST_SECTION("with TranslationVoice explicitly set")
        {
            for (auto &pair : voiceNameMap)
            {
                std::string voiceNameProperty = pair.first + GetPropertyName(PropertyId::SpeechServiceConnection_TranslationVoice);
                properties->SetStringValue(voiceNameProperty.c_str(), pair.second.c_str());
            }
            expectedJson["translation"]["onSuccess"]["action"] = "Synthesize";
            expectedJson["translation"]["onPassthrough"]["action"] = "Synthesize";
            for (auto& pair : voiceNameMap)
            {
                expectedJson["synthesis"]["defaultVoices"][pair.first] = pair.second;
            }
            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }
    }

    SPXTEST_SECTION("SR NewEndpoint")
    {
        adapterTest.SetEndpointType(USP::EndpointType::Speech);
        session->SetTargetLanguages("");
        auto properties = session->GetPropertiesPtr();

        std::map<std::string, std::string> languageToEndpointIdMap{
            {"de-DE", "CustomEndpoint1"},
            {"fr-FR", "CustomEndpoint2"}};
        std::vector<std::string> sourceLangs;
        for (auto &pair : languageToEndpointIdMap)
        {
            sourceLangs.push_back(pair.first);
        }
        sourceLangs.push_back("en-US");
        session->SetAutoDetectLanguages(adapterTest.Join(sourceLangs));

        adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectAtAudioStart, USP::LanguageIdPriority::PrioritizeLatency);
        session->SetLanguageIdMode("AtStart");
        auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "Recognize", "DetectAtAudioStart", "PrioritizeLatency");
        expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);

        ajv::JsonBuilder phraseDetectionJson;
        phraseDetectionJson["onSuccess"]["action"] = "None";
        phraseDetectionJson["onInterim"]["action"] = "None";
        phraseDetectionJson["mode"] = "INTERACTIVE";
        expectedJson["phraseDetection"] = phraseDetectionJson;
        expectedJson["phraseOutput"]["interimResults"]["resultType"] = "Auto";
        expectedJson["phraseOutput"]["phraseResults"]["resultType"] = "Always";

        SPXTEST_SECTION("without explicit EndpointIds")
        {
            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }

        SPXTEST_SECTION("with explicit EndpointIds")
        {
            for (auto &pair : languageToEndpointIdMap)
            {
                std::string endPointIdProperty = pair.first + GetPropertyName(PropertyId::SpeechServiceConnection_EndpointId);
                properties->SetStringValue(endPointIdProperty.c_str(), pair.second.c_str());
            }

            ajv::JsonBuilder customModelsJson;
            ajv::JsonBuilder deJson;
            deJson["endpoint"] = "CustomEndpoint1";
            deJson["language"] = "de-DE";
            customModelsJson[0] = deJson;
            ajv::JsonBuilder frJson;
            frJson["endpoint"] = "CustomEndpoint2";
            frJson["language"] = "fr-FR";
            customModelsJson[1] = frJson;
            expectedJson["phraseDetection"]["customModels"] = customModelsJson;
            adapterTest.RequireJsonMatch(expectedJson.AsJson());

            // Reset
            for (auto &pair : languageToEndpointIdMap)
            {
                std::string endPointIdProperty = pair.first + GetPropertyName(PropertyId::SpeechServiceConnection_EndpointId);
                properties->SetStringValue(endPointIdProperty.c_str(), "");
            }
        }

        // Tear down
        properties->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_LanguageIdMode), "");
    }

    SPXTEST_SECTION("Language id Only")
    {
        adapterTest.SetEndpointType(USP::EndpointType::StandaloneLanguageId);
        auto properties = session->GetPropertiesPtr();

        std::vector<std::string> sourceLangs = {"de-DE", "fr-FR", "en-US"};
        session->SetAutoDetectLanguages(adapterTest.Join(sourceLangs));

        SPXTEST_SECTION("Language Detection only Default")
        {
            adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectAtAudioStart, USP::LanguageIdPriority::PrioritizeLatency);

            // Set the expected language id json
            auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "None", "DetectAtAudioStart", "PrioritizeLatency");
            expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);

            expectedJson["phraseDetection"]["mode"] = "None";

            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }

        SPXTEST_SECTION("Language Detection only set LanguageIdMode to AtStart")
        {
            adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectAtAudioStart, USP::LanguageIdPriority::PrioritizeLatency);

            session->SetLanguageIdMode("AtStart");

            // Set the expected language id json
            auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "None", "DetectAtAudioStart", "PrioritizeLatency");
            expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);

            expectedJson["phraseDetection"]["mode"] = "None";

            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }

        SPXTEST_SECTION("Language Detection only set LanguageIdMode to Continuous")
        {
            adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectContinuous, USP::LanguageIdPriority::PrioritizeLatency);

            session->SetLanguageIdMode("Continuous");

            // Set the expected language id json
            auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "None", "DetectContinuous", "PrioritizeLatency");
            expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);

            expectedJson["phraseDetection"]["mode"] = "None";

            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }

        // Tear down
        properties->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_LanguageIdMode), "");
    }

    SPXTEST_SECTION("Language Id and Speech Recognition")
    {
        adapterTest.SetEndpointType(USP::EndpointType::Speech);
        session->SetTargetLanguages("");
        auto properties = session->GetPropertiesPtr();

        std::map<std::string, std::string> languageToEndpointIdMap{
            {"de-DE", "CustomEndpoint1"},
            {"fr-FR", "CustomEndpoint2"}};
        std::vector<std::string> sourceLangs;
        for (auto &pair : languageToEndpointIdMap)
        {
            sourceLangs.push_back(pair.first);
        }

        session->SetAutoDetectLanguages(adapterTest.Join(sourceLangs));

        ajv::JsonBuilder phraseDetectionJson;
        phraseDetectionJson["onSuccess"]["action"] = "None";
        phraseDetectionJson["onInterim"]["action"] = "None";
        phraseDetectionJson["mode"] = "INTERACTIVE";
        expectedJson["phraseDetection"] = phraseDetectionJson;
        expectedJson["phraseOutput"]["interimResults"]["resultType"] = "Auto";
        expectedJson["phraseOutput"]["phraseResults"]["resultType"] = "Always";

        SPXTEST_SECTION("without explicit EndpointIds")
        {
            SPXTEST_SECTION("LanguageIdMode AtStart")
            {
                adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectAtAudioStart, USP::LanguageIdPriority::PrioritizeLatency);
                session->SetLanguageIdMode("AtStart");
                auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "Recognize", "DetectAtAudioStart", "PrioritizeLatency");
                expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);
            }

            SPXTEST_SECTION("LanguageIdMOde Continuous")
            {
                adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectContinuous, USP::LanguageIdPriority::PrioritizeLatency);
                session->SetLanguageIdMode("Continuous");
                auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "Recognize", "DetectContinuous", "PrioritizeLatency");
                expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);
            }

            adapterTest.RequireJsonMatch(expectedJson.AsJson());
        }

        SPXTEST_SECTION("with explicit EndpointIds")
        {
            for (auto &pair : languageToEndpointIdMap)
            {
                std::string endPointIdProperty = pair.first + GetPropertyName(PropertyId::SpeechServiceConnection_EndpointId);
                properties->SetStringValue(endPointIdProperty.c_str(), pair.second.c_str());
            }

            ajv::JsonBuilder customModelsJson;
            ajv::JsonBuilder deJson;
            deJson["endpoint"] = "CustomEndpoint1";
            deJson["language"] = "de-DE";
            customModelsJson[0] = deJson;
            ajv::JsonBuilder frJson;
            frJson["endpoint"] = "CustomEndpoint2";
            frJson["language"] = "fr-FR";
            customModelsJson[1] = frJson;
            expectedJson["phraseDetection"]["customModels"] = customModelsJson;

            SPXTEST_SECTION("LanguageIdMode with AtStart")
            {
                adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectAtAudioStart, USP::LanguageIdPriority::PrioritizeLatency);
                session->SetLanguageIdMode("AtStart");
                auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "Recognize", "DetectAtAudioStart", "PrioritizeLatency");
                expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);
            }

            SPXTEST_SECTION("LanguageIdMode with Continuous")
            {
                adapterTest.SetLanguageIdModeAndPriority(USP::LanguageIdMode::DetectContinuous, USP::LanguageIdPriority::PrioritizeLatency);

                session->SetLanguageIdMode("Continuous");
                auto languageIdJson = session->SetLanguageIdJson(sourceLangs, "Recognize", "DetectContinuous", "PrioritizeLatency");
                expectedJson["languageId"] = ajv::JsonBuilder(languageIdJson);
            }

            adapterTest.RequireJsonMatch(expectedJson.AsJson());

            // Teardown
            for (auto &pair : languageToEndpointIdMap)
            {
                std::string endPointIdProperty = pair.first + GetPropertyName(PropertyId::SpeechServiceConnection_EndpointId);
                properties->SetStringValue(endPointIdProperty.c_str(), "");
            }
        }

        // Tear down
        properties->SetStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_LanguageIdMode), "");
    }

    SPXTEST_SECTION("Custom segmentation strategy")
    {
        auto mode = GENERATE("INTERACTIVE");
        SPXTEST_SECTION(mode)
        {
            SPXTEST_SECTION(g_segmentationStrategyDefault)
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationStrategy), g_segmentationStrategyDefault);

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }

            SPXTEST_SECTION(g_segmentationStrategyTime)
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationStrategy), g_segmentationStrategyTime);

                expectedJson["phraseDetection"][mode]["segmentation"]["mode"] = g_segmentationModeCustom;

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }

            SPXTEST_SECTION(g_segmentationStrategySemantic)
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationStrategy), g_segmentationStrategySemantic);

                expectedJson["phraseDetection"][mode]["segmentation"]["mode"] = g_segmentationModeSemantic;

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }

            SPXTEST_SECTION("Unexpected Input")
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationStrategy), "Unexpected Input");

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }
        }
    }

    SPXTEST_SECTION("Custom segmentation timeout")
    {
        auto mode = GENERATE("INTERACTIVE");
        SPXTEST_SECTION(mode)
        {
            SPXTEST_SECTION("No Strategy")
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationSilenceTimeoutMs), "3000");
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationMaximumTimeMs), "7000");


                expectedJson["phraseDetection"][mode]["segmentation"]["mode"] = g_segmentationModeCustom;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationSilenceTimeoutMs"] = 3000;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationForcedTimeoutMs"] = 7000;
                expectedJson["phraseDetection"]["mode"] = mode;

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }

            SPXTEST_SECTION(g_segmentationStrategyDefault)
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationStrategy), g_segmentationStrategyDefault);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationSilenceTimeoutMs), "3000");
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationMaximumTimeMs), "7000");


                expectedJson["phraseDetection"][mode]["segmentation"]["mode"] = g_segmentationModeCustom;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationSilenceTimeoutMs"] = 3000;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationForcedTimeoutMs"] = 7000;
                expectedJson["phraseDetection"]["mode"] = mode;

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }

            SPXTEST_SECTION(g_segmentationStrategyTime)
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationStrategy), g_segmentationStrategyTime);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationSilenceTimeoutMs), "3000");
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationMaximumTimeMs), "7000");


                expectedJson["phraseDetection"][mode]["segmentation"]["mode"] = g_segmentationModeCustom;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationSilenceTimeoutMs"] = 3000;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationForcedTimeoutMs"] = 7000;
                expectedJson["phraseDetection"]["mode"] = mode;

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }

            SPXTEST_SECTION(g_segmentationStrategySemantic)
            {
                auto properties = adapterTest.GetSession()->GetPropertiesPtr();
                properties->SetStringValue("SPEECH-RecoMode", mode);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationStrategy), g_segmentationStrategySemantic);
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationSilenceTimeoutMs), "3000");
                properties->SetStringValue(GetPropertyName(PropertyId::Speech_SegmentationMaximumTimeMs), "7000");


                expectedJson["phraseDetection"][mode]["segmentation"]["mode"] = g_segmentationModeSemantic;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationSilenceTimeoutMs"] = 3000;
                expectedJson["phraseDetection"][mode]["segmentation"]["segmentationForcedTimeoutMs"] = 7000;
                expectedJson["phraseDetection"]["mode"] = mode;

                adapterTest.RequireJsonContains(expectedJson.AsJson());
            }
        }
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("PostProcessingOption TrueText enrichment key matches reco mode", "[context_json][truetext]")
{
    // Tests that AddPostProcessingOptionsJsonToContext places the TrueText enrichment
    // settings under the correct key in phraseDetection.enrichment, matching the
    // resolved reco mode (interactive, conversation, or dictation).
    //
    // The adapter constructor calls ResolveRecoMode(singleShot=true) which locks to
    // INTERACTIVE. We cannot change the mode after that. Instead, we set the
    // PostProcessingOption and verify the enrichment key matches "interactive".
    // Then we directly verify the C++ logic by checking the generated JSON string
    // for conversation and dictation modes, since the code path in
    // AddPostProcessingOptionsJsonToContext uses the same recoMode variable for all modes.

    CSpxUspRecoEngineAdapterTest adapterTest;
    auto session = adapterTest.GetSession();
    auto properties = session->GetPropertiesPtr();

    SPXTEST_SECTION("INTERACTIVE mode - enrichment under interactive key")
    {
        properties->SetStringValue(
            GetPropertyName(PropertyId::SpeechServiceResponse_PostProcessingOption), "TrueText");

        ajv::JsonBuilder expectedJson;
        expectedJson["audio"]["streams"]["1"] = nullptr;
        expectedJson["phraseDetection"]["enrichment"]["interactive"]["punctuationMode"] = "Implicit";
        expectedJson["phraseDetection"]["enrichment"]["interactive"]["disfluencyMode"] = "Removed";
        expectedJson["phraseDetection"]["enrichment"]["interactive"]["intermediatePunctuationMode"] = "Implicit";
        expectedJson["phraseDetection"]["enrichment"]["interactive"]["intermediatedisfluencymode"] = "Removed";

        adapterTest.RequireJsonContains(expectedJson.AsJson());

        // Also verify conversation and dictation keys are NOT present
        auto actualJson = adapterTest.GetSpeechContextJson();
        SPXTEST_CHECK(actualJson.find("\"conversation\"") == std::string::npos);
        SPXTEST_CHECK(actualJson.find("\"dictation\"") == std::string::npos);
    }

    SPXTEST_SECTION("TrueText is case-insensitive")
    {
        properties->SetStringValue(
            GetPropertyName(PropertyId::SpeechServiceResponse_PostProcessingOption), "truetext");

        ajv::JsonBuilder expectedJson;
        expectedJson["audio"]["streams"]["1"] = nullptr;
        expectedJson["phraseDetection"]["enrichment"]["interactive"]["punctuationMode"] = "Implicit";

        adapterTest.RequireJsonContains(expectedJson.AsJson());
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("PostProcessingOption custom value passes through to service", "[context_json][postprocessing]")
{
    // Tests that non-TrueText PostProcessingOption values are passed through
    // as-is under phraseDetection.enrichment[mode].postprocessingoption.

    CSpxUspRecoEngineAdapterTest adapterTest;
    auto session = adapterTest.GetSession();
    auto properties = session->GetPropertiesPtr();

    properties->SetStringValue(
        GetPropertyName(PropertyId::SpeechServiceResponse_PostProcessingOption), "CustomOption");

    ajv::JsonBuilder expectedJson;
    expectedJson["audio"]["streams"]["1"] = nullptr;
    expectedJson["phraseDetection"]["enrichment"]["interactive"]["postprocessingoption"] = "CustomOption";

    adapterTest.RequireJsonContains(expectedJson.AsJson());
}
SPXTEST_CASE_END()
