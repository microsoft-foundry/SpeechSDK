//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

// <code>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

#include <speechapi_cxx.h>

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Audio;
using namespace Microsoft::CognitiveServices::Speech::Diagnostics::Logging;
using namespace Microsoft::CognitiveServices::Speech::Translation;


// Get a current timestamp.
static const std::string CurrentTime(const std::string& format)
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto now_tt = system_clock::to_time_t(now);

    std::tm now_tm;
#ifdef _MSC_VER
    localtime_s(&now_tm, &now_tt);
#else
    now_tm = *std::localtime(&now_tt);
#endif

    std::ostringstream oss;
    oss << std::put_time(&now_tm, format.c_str());
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

static const std::string CurrentTime()
{
    return CurrentTime("%H:%M:%S");
}


// Implements a pull stream callback that the SDK uses to read input audio
// samples for speech-to-text (recognition, translation).
class PullStreamInputReader final : public PullAudioInputStreamCallback
{
private:
    std::ifstream m_input;

public:
    PullStreamInputReader(const std::string& inputFile)
    {
        // In this example the input stream is a file. Modify the code to use
        // a non-file source (e.g. API that returns audio data) as necessary.
        m_input.open(inputFile, std::ios::in | std::ios::binary);
        if (!m_input.good())
        {
            throw std::invalid_argument("Failed to open file " + inputFile);
        }
    }

    // This method is called to synchronously get data (at most 'size' bytes)
    // from the input stream.
    // It must return the number of bytes copied into the data buffer.
    // If there is no data, the method must either wait until data becomes
    // available or return 0 to indicate the end of stream.
    int Read(uint8_t* buffer, uint32_t size) override
    {
        // Copy audio data from the input stream into a data buffer for the
        // Speech SDK to consume.
        // Data must NOT include any headers. Everything in the input is
        // considered and processed as raw PCM audio samples. The format
        // is configured with AudioStreamFormat::GetWaveFormatPCM(...).
        m_input.read((char*)buffer, size);
        std::streamsize bytesRead = m_input.gcount();

        // If the method used to read the input stream can return
        // a negative number of bytes in case of an error, be sure to
        // check the value. Do not pass a negative number to the SDK.
        return (bytesRead >= 0) ? (int)bytesRead : 0;
    }

    // This method is called for clean-up of resources at the end of stream.
    void Close() override
    {
        m_input.close();
    }
};


//
// Recognize speech until the end of input stream from a file.
// See more examples in
// https://github.com/Azure-Samples/cognitive-services-speech-sdk/blob/master/samples/cpp/windows/console/samples/speech_recognition_samples.cpp
// Supported languages:
// https://learn.microsoft.com/en-us/azure/ai-services/speech-service/language-support?tabs=stt
//
void RecognizeSpeech(
    const std::string& serviceRegion,
    const std::string& subscriptionKey,
    const std::string& inputLocale,
    const std::string& inputFile)
{
    // Create an instance of speech config.
    auto speechConfig = SpeechConfig::FromSubscription(subscriptionKey, serviceRegion);

    // Set recognition language.
    speechConfig->SetSpeechRecognitionLanguage(inputLocale);

    // Create an instance of audio config using a pull stream for input.
    auto audioFormat = AudioStreamFormat::GetWaveFormatPCM(16000, (uint8_t)16, (uint8_t)1);
    auto pullStreamCallback = std::make_shared<PullStreamInputReader>(inputFile);
    auto pullStream = AudioInputStream::CreatePullStream(audioFormat, pullStreamCallback);
    auto audioConfig = AudioConfig::FromStreamInput(pullStream);

    // Create an instance of a speech recognizer with the given configs.
    auto recognizer = SpeechRecognizer::FromConfig(speechConfig, audioConfig);

    std::promise<void> recognitionEnd;

    // Subscribe to events.
    recognizer->Recognizing.Connect([](const SpeechRecognitionEventArgs& e)
    {
        // Intermediate result (hypothesis).
        if (e.Result->Reason == ResultReason::RecognizingSpeech)
        {
            std::cout << CurrentTime() << " "
                << "Recognizing: " << e.Result->Text << std::endl;
        }
    });

    recognizer->Recognized.Connect([](const SpeechRecognitionEventArgs& e)
    {
        if (e.Result->Reason == ResultReason::RecognizedSpeech)
        {
            // Final result. May differ from the last intermediate result.
            std::cout << CurrentTime() << " "
                << "RECOGNIZED:  " << e.Result->Text
                // Unit of audio Offset and Duration is tick (1 tick = 100 nanoseconds).
                << " [ " << e.Result->Offset() / 10000 << " + " << e.Result->Duration() / 10000
                << " = " << (e.Result->Offset() + e.Result->Duration()) / 10000 << " ms ]"
                << std::endl;
        }
        else if (e.Result->Reason == ResultReason::NoMatch)
        {
            // NoMatch occurs when no speech phrase was recognized.
            std::cout << CurrentTime() << " "
                << "NO MATCH: Reason="
                << (NoMatchDetails::FromResult(e.Result)->Reason == NoMatchReason::InitialSilenceTimeout ? "SilenceTimeout" : "NotRecognized")
                << " [ " << e.Result->Offset() / 10000 << " + " << e.Result->Duration() / 10000
                << " = " << (e.Result->Offset() + e.Result->Duration()) / 10000 << " ms ]"
                << std::endl;
        }
    });

    recognizer->Canceled.Connect([](const SpeechRecognitionCanceledEventArgs& e)
    {
        std::cout << CurrentTime() << " "
            << "CANCELED: Reason=" << (e.Reason == CancellationReason::EndOfStream ? "EndOfStream" : "Error");

        if (e.Reason == CancellationReason::Error)
        {
            // NOTE: In case of an error, do not try using the same recognizer instance anymore.
            std::cout << " ErrorCode=" << (int)e.ErrorCode << " ErrorDetails=" << e.ErrorDetails;
        }
        std::cout << std::endl;
    });

    recognizer->SpeechStartDetected.Connect([](const RecognitionEventArgs& e)
    {
        std::cout << CurrentTime() << " "
            << "Speech start detected [ " << e.Offset / 10000 << " ms ]" << std::endl;
    });

    recognizer->SpeechEndDetected.Connect([](const RecognitionEventArgs& e)
    {
        std::cout << CurrentTime() << " "
            << "Speech end detected [ " << e.Offset / 10000 << " ms ]" << std::endl;
    });

    recognizer->SessionStarted.Connect([](const SessionEventArgs& e)
    {
        std::cout << CurrentTime() << " "
            << "SESSION STARTED: SessionId=" << e.SessionId << std::endl;
    });

    recognizer->SessionStopped.Connect([&recognitionEnd](const SessionEventArgs& e)
    {
        std::cout << CurrentTime() << " "
            << "SESSION STOPPED: SessionId=" << e.SessionId << std::endl;
        recognitionEnd.set_value();
    });

    // The following lines run continuous recognition that listens for speech
    // in input audio and generates results until stopped. To run recognition
    // only once (until there's recognized speech or a timeout), replace this
    // code block with
    //
    // auto result = recognizer->RecognizeOnceAsync().get();

    recognizer->StartContinuousRecognitionAsync().get();
    recognitionEnd.get_future().get();
    recognizer->StopContinuousRecognitionAsync().get();
}


//
// Synthesize speech to an audio data stream and a wav file.
// See more examples in
// https://github.com/Azure-Samples/cognitive-services-speech-sdk/blob/master/samples/cpp/windows/console/samples/speech_synthesis_samples.cpp
// Supported languages:
// https://learn.microsoft.com/en-us/azure/ai-services/speech-service/language-support?tabs=tts
//
void SynthesizeSpeech(
    const std::string& serviceRegion,
    const std::string& subscriptionKey,
    const std::string& inputLocale,
    const std::string& inputText)
{
    // Create an instance of speech config.
    auto config = SpeechConfig::FromSubscription(subscriptionKey, serviceRegion);

    // Set speech synthesis language.
    config->SetSpeechSynthesisLanguage(inputLocale);

    // Create a speech synthesizer with a null output stream.
    // This means the audio output data will not be written to a loudspeaker or a push/pull stream.
    // You can just get the audio from the result.
    auto synthesizer = SpeechSynthesizer::FromConfig(config, nullptr);

    // Subscribe to events.
    synthesizer->SynthesisStarted += [](const SpeechSynthesisEventArgs& e)
        {
            UNUSED(e);
            std::cout << CurrentTime() << " "
                << "Synthesis started" << std::endl;
        };

    synthesizer->Synthesizing += [](const SpeechSynthesisEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "Synthesizing, received audio " << e.Result->GetAudioLength() << " bytes" << std::endl;
        };

    synthesizer->SynthesisCompleted += [](const SpeechSynthesisEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "Synthesis completed" << std::endl;
        };

    synthesizer->WordBoundary += [](const SpeechSynthesisWordBoundaryEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "Word \"" << e.Text << "\" | "
                << "Text offset " << e.TextOffset << " | "
                // Unit of AudioOffset is tick (1 tick = 100 nanoseconds).
                << "Audio offset " << (e.AudioOffset + 5000) / 10000 << " ms"
                << std::endl;
        };

    // Synthesize speech and wait for the result.
    auto resultFuture = synthesizer->SpeakTextAsync(inputText);
    auto result = resultFuture.get();

    // Check the result.
    if (result->Reason == ResultReason::SynthesizingAudioCompleted)
    {
        std::cout << CurrentTime() << " "
            << "SYNTHESIZED speech for text \"" << inputText << "\"" << std::endl;
        auto audioDataStream = AudioDataStream::FromResult(result);

        // You can save all the data in the audio data stream to a file
        std::string wavFilename = "SynthesizedSpeech_" + CurrentTime("%Y-%m-%d_%H-%M-%S") + ".wav";
        audioDataStream->SaveToWavFile(wavFilename);
        std::cout << CurrentTime() << " "
            << "Saved " << wavFilename << std::endl;

        // You can also read data from audio data stream and process it in memory:

        // Reset the stream position back to the beginning in case the audio was saved to a file.
        audioDataStream->SetPosition(0);

        uint8_t buffer[16000];
        uint32_t totalSize = 0;
        uint32_t filledSize = 0;

        std::cout << CurrentTime() << " "
            << "Reading AudioDataStream..." << std::endl;
        while ((filledSize = audioDataStream->ReadData(buffer, sizeof(buffer))) > 0)
        {
            std::cout << CurrentTime() << " "
                << filledSize << " bytes read" << std::endl;
            totalSize += filledSize;
        }

        std::cout << CurrentTime() << " "
            << totalSize << " bytes total" << std::endl;
    }
    else if (result->Reason == ResultReason::Canceled)
    {
        auto cancellation = SpeechSynthesisCancellationDetails::FromResult(result);
        std::cout << CurrentTime() << " "
            << "CANCELED: Reason=" << (int)cancellation->Reason;

        if (cancellation->Reason == CancellationReason::Error)
        {
            // NOTE: In case of an error, do not try using the same synthesizer instance anymore.
            std::cout << " ErrorCode=" << (int)cancellation->ErrorCode << " ErrorDetails=" << cancellation->ErrorDetails;
        }
        std::cout << std::endl;
    }
}


//
// Translate speech until the end of input stream from a file.
// See more examples in
// https://github.com/Azure-Samples/cognitive-services-speech-sdk/blob/master/samples/cpp/windows/console/samples/translation_samples.cpp
// Supported languages:
// https://learn.microsoft.com/en-us/azure/ai-services/speech-service/language-support?tabs=speech-translation
//
void TranslateSpeech(
    const std::string& serviceRegion,
    const std::string& subscriptionKey,
    const std::string& inputLocale,
    const std::string& inputFile,
    const std::string& targetLanguage)
{
    // Create an instance of translation config.
    auto speechConfig = SpeechTranslationConfig::FromSubscription(subscriptionKey, serviceRegion);

    // Set recognition and translation languages.
    speechConfig->SetSpeechRecognitionLanguage(inputLocale);
    speechConfig->AddTargetLanguage(targetLanguage);

    // Create an instance of audio config.
    auto audioFormat = AudioStreamFormat::GetWaveFormatPCM(16000, (uint8_t)16, (uint8_t)1);
    auto pullStreamCallback = std::make_shared<PullStreamInputReader>(inputFile);
    auto pullStream = AudioInputStream::CreatePullStream(audioFormat, pullStreamCallback);
    auto audioConfig = AudioConfig::FromStreamInput(pullStream);

    // Create an instance of a translation recognizer with the given configs.
    auto recognizer = TranslationRecognizer::FromConfig(speechConfig, audioConfig);

    std::promise<void> recognitionEnd;

    // Subscribe to events.
    recognizer->Recognizing.Connect([](const TranslationRecognitionEventArgs& e)
        {
            // Intermediate result (hypothesis).
            if (e.Result->Reason == ResultReason::TranslatingSpeech)
            {
                std::cout << CurrentTime() << " "
                    << "Recognizing: " << e.Result->Text << std::endl;
                for (const auto& pair : e.Result->Translations)
                {
                    std::cout << CurrentTime() << " "
                        << "Translating: [" << pair.first << "] " << pair.second << std::endl;
                }
            }
        });

    recognizer->Recognized.Connect([](const TranslationRecognitionEventArgs& e)
        {
            if (e.Result->Reason == ResultReason::TranslatedSpeech)
            {
                // Final result. May differ from the last intermediate result.
                std::cout << CurrentTime() << " "
                    << "RECOGNIZED:  " << e.Result->Text
                    // Unit of audio Offset and Duration is tick (1 tick = 100 nanoseconds).
                    << " [ " << e.Result->Offset() / 10000 << " + " << e.Result->Duration() / 10000
                    << " = " << (e.Result->Offset() + e.Result->Duration()) / 10000 << " ms ]"
                    << std::endl;

                for (const auto& pair : e.Result->Translations)
                {
                    std::cout << CurrentTime() << " "
                        << "TRANSLATED:  [" << pair.first << "] " << pair.second << std::endl;
                }
            }
            else if (e.Result->Reason == ResultReason::NoMatch)
            {
                // NoMatch occurs when no speech phrase was recognized.
                std::cout << CurrentTime() << " "
                    << "NO MATCH: Reason="
                    << (NoMatchDetails::FromResult(e.Result)->Reason == NoMatchReason::InitialSilenceTimeout ? "SilenceTimeout" : "NotRecognized")
                    << " [ " << e.Result->Offset() / 10000 << " + " << e.Result->Duration() / 10000
                    << " = " << (e.Result->Offset() + e.Result->Duration()) / 10000 << " ms ]"
                    << std::endl;
            }
        });

    recognizer->Canceled.Connect([](const TranslationRecognitionCanceledEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "CANCELED: Reason=" << (e.Reason == CancellationReason::EndOfStream ? "EndOfStream" : "Error");

            if (e.Reason == CancellationReason::Error)
            {
                // NOTE: In case of an error, do not try using the same recognizer instance anymore.
                std::cout << " ErrorCode=" << (int)e.ErrorCode << " ErrorDetails=" << e.ErrorDetails;
            }
            std::cout << std::endl;
        });

    recognizer->SpeechStartDetected.Connect([](const RecognitionEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "Speech start detected [ " << e.Offset / 10000 << " ms ]" << std::endl;
        });

    recognizer->SpeechEndDetected.Connect([](const RecognitionEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "Speech end detected [ " << e.Offset / 10000 << " ms ]" << std::endl;
        });

    recognizer->SessionStarted.Connect([](const SessionEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "SESSION STARTED: SessionId=" << e.SessionId << std::endl;
        });

    recognizer->SessionStopped.Connect([&recognitionEnd](const SessionEventArgs& e)
        {
            std::cout << CurrentTime() << " "
                << "SESSION STOPPED: SessionId=" << e.SessionId << std::endl;
            recognitionEnd.set_value();
        });

    // The following lines run continuous recognition that listens for speech
    // in input audio and generates results until stopped. To run recognition
    // only once (until there's recognized speech or a timeout), replace this
    // code block with
    //
    // auto result = recognizer->RecognizeOnceAsync().get();

    recognizer->StartContinuousRecognitionAsync().get();
    recognitionEnd.get_future().get();
    recognizer->StopContinuousRecognitionAsync().get();
}


static void ShowUsage(const std::string& note = "")
{
    if (!note.empty())
    {
        std::cerr << note << std::endl;
    }
    printf(
        "USAGE: sdkdemo <command> [...]\n"
        "\n"
        "COMMANDS\n"
        "\n"
        "  recognize <options>     Speech to text.\n"
        "  synthesize <options>    Text to speech.\n"
        "  translate <options>     Speech to text in specific target language.\n"
        "\n"
        " Options, common\n"
        "  -locale LOCALE          Input locale in BCP 47 format. (default: en-US)\n"
        "\n"
        " Options for 'recognize'\n"
        "  -file FILENAME          Input audio filename.\n"
        "                          Audio must be 16-bit 16-kHz PCM-encoded mono.\n"
        "                          File should be raw audio data with no format header.\n"
        "\n"
        " Options for 'synthesize'\n"
        "  -text TEXT              Input text string.\n"
        "                          Use \" and \" around a string with multiple words.\n"
        "\n"
        " Options for 'translate'\n"
        "  -file FILENAME          Input audio filename.\n"
        "                          Audio must be 16-bit 16-kHz PCM-encoded mono.\n"
        "                          File should be raw audio data with no format header.\n"
        "  -targetLang LANGUAGE    Translation target language. (default: en)\n"
        "\n"
        "ENVIRONMENT\n"
        "\n"
        "  Set your speech service region (e.g. westus) and subscription key in\n"
        "  environment variables SPEECH_SERVICE_REGION and SPEECH_SUBSCRIPTION_KEY.\n"
        "\n"
        "  Linux or macOS:\n"
        "    export SPEECH_SERVICE_REGION=YourServiceRegion\n"
        "    export SPEECH_SUBSCRIPTION_KEY=YourSubscriptionKey\n"
        "\n"
        "  Windows:\n"
        "    set SPEECH_SERVICE_REGION=YourServiceRegion\n"
        "    set SPEECH_SUBSCRIPTION_KEY=YourSubscriptionKey\n"
        "\n"
        "EXAMPLES\n"
        "\n"
        "  sdkdemo recognize -file english.pcm\n"
        "  sdkdemo recognize -file german.pcm -locale de-DE\n"
        "\n"
        "  sdkdemo synthesize -text \"What's the weather like?\"\n"
        "  sdkdemo synthesize -text \"Wie ist das Wetter?\" -locale de-DE\n"
        "\n"
        "  sdkdemo translate -file english.pcm -targetLang de\n"
        "  sdkdemo translate -file german.pcm -locale de-DE -targetLang en\n"
    );
}


// Read an environment variable.
static const char* getEnvVar(const char* var)
{
#pragma warning(suppress : 4996) // getenv
    const char* val = getenv(var);
    if (val == nullptr)
    {
        fprintf(stderr, "Environment variable not set: %s\n", var);
    }
    return val;
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        ShowUsage();
        return 0;
    }

    std::string command;
    std::string inputLocale = "en-US";
    std::string inputFile;
    std::string inputText;
    std::string targetLanguage = "en";

    // Parse command-line arguments.
    for (auto i = 1; i < argc; i++)
    {
        std::string str(argv[i]);
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });

        if (str == "recognize" || str == "synthesize" || str == "translate")
        {
            command = str;
        }
        else if (str == "-locale" && i + 1 < argc)
        {
            inputLocale = argv[++i];
        }
        else if (str == "-file" && i + 1 < argc)
        {
            inputFile = argv[++i];
        }
        else if (str == "-text" && i + 1 < argc)
        {
            inputText = argv[++i];
        }
        else if (str == "-targetlang" && i + 1 < argc)
        {
            targetLanguage = argv[++i];
        }
        else
        {
            fprintf(stderr, "Unknown or redundant argument, ignored: %s\n", argv[i]);
        }
    }

    if (command.empty())
    {
        ShowUsage("No command specified!");
        return 1;
    }

    if (inputFile.empty() && inputText.empty())
    {
        ShowUsage("No input specified!");
        return 2;
    }

    try
    {
        // Get speech service subscription settings from environment.
        const char* serviceRegion = getEnvVar("SPEECH_SERVICE_REGION");
        const char* subscriptionKey = getEnvVar("SPEECH_SUBSCRIPTION_KEY");
        if (!serviceRegion || !subscriptionKey)
        {
            return 3;
        }

        // Start SDK logging to a file.
        std::string logFilename = "speechsdk_" + CurrentTime("%Y-%m-%d_%H-%M-%S") + ".log";
        FileLogger::Start(logFilename);

        // Run the use case.
        if (command == "recognize")
        {
            RecognizeSpeech(serviceRegion, subscriptionKey, inputLocale, inputFile);
        }
        else if (command == "synthesize")
        {
            SynthesizeSpeech(serviceRegion, subscriptionKey, inputLocale, inputText);
        }
        else if (command == "translate")
        {
            TranslateSpeech(serviceRegion, subscriptionKey, inputLocale, inputFile, targetLanguage);
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        FileLogger::Stop(); // Optional; the log file is written when the process exits normally.
        return 4;
    }

    return 0;
}
// </code>
