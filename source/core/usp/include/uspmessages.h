//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// uspmessages.h: definition of USP messages that are exposed to users.
//
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <map>
#include <list>
#include "ispxinterfaces.h"
#include "recognition_status.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

typedef uint64_t OffsetType;
typedef uint64_t DurationType;

/**
 * Represents keyword status in speech keyword.
 */
enum class KeywordVerificationStatus : int
{
    Accepted,
    Rejected,
    InvalidMessage
};

/**
* Represents translation status in translation phrase.
*/
enum class TranslationStatus : int
{
    Success, Error, InvalidMessage
};

/**
* Represents the confidence level of language detection result
*/
enum class ConfidenceLevel : int
{
    Unknown = 1,
    Low,
    Medium,
    High,
    InvalidMessage
};

struct JsonMsg
{

    JsonMsg() = default;

    std::string json;

protected:
    JsonMsg(std::string&& content) :
        json(std::move(content))
    {}
};

/**
 * Represents speech.startDectected message
 */
struct SpeechStartDetectedMsg : public JsonMsg
{
    SpeechStartDetectedMsg(std::string&& content, OffsetType offset) :
        JsonMsg(std::move(content)),
        offset(offset)
    {}

    OffsetType offset { 0 };
};

/**
* Represents speech.endDetected message
*/
struct SpeechEndDetectedMsg : public JsonMsg
{
    SpeechEndDetectedMsg(std::string&& content, OffsetType offset) :
        JsonMsg(std::move(content)),
        offset(offset)
    {}

    OffsetType offset{ 0 };
};

/**
* Represents turn.start message
*/
struct TurnStartMsg : public JsonMsg
{
    TurnStartMsg(std::string&& content, const std::string& tag, const std::string& request, bool serviceManagesOffset, OffsetType offset) :
        JsonMsg(std::move(content)),
        contextServiceTag(tag),
        requestId{ request },
        serviceManagesOffset(serviceManagesOffset),
        offset(offset)
    {
    }

    std::string contextServiceTag;
    std::string requestId;
    bool serviceManagesOffset;
    OffsetType offset{ 0 };
};

/**
* Represents turn.end message
* Note: Body is empty.
*/
struct TurnEndMsg : JsonMsg
{
    TurnEndMsg(const std::string& request) :
        JsonMsg(std::string()),
        requestId{ request }
    {}

    std::string requestId;
};

struct SpeechMsg : public JsonMsg
{
    SpeechMsg() = default;
    SpeechMsg(std::string&& content, OffsetType offset, DurationType duration, std::string&& speaker = "", std::string&& utteranceId = "", std::string&& phraseId = "") :
        JsonMsg(std::move(content)),
        offset(offset),
        duration(duration),
        speaker(std::move(speaker)),
        utteranceId(std::move(utteranceId)),
        phraseId(std::move(phraseId))
    {}

    OffsetType offset{ 0 };
    DurationType duration{ 0 };
    std::string speaker{ "" };
    std::string utteranceId{ "" };
    std::string phraseId{ "" };
};

/**
 * Represents speech.hypothesis message
 */
struct SpeechHypothesisMsg : public SpeechMsg
{
    SpeechHypothesisMsg(
        std::string&& content,
        OffsetType offset,
        DurationType duration,
        std::string&& text,
        std::string&& speaker = "",
        std::string&& id = "",
        std::string&& language = "",
        std::string&& phraseId = "",
        bool isTentative = false) :
        SpeechMsg(std::move(content), offset, duration, std::move(speaker), std::move(id), std::move(phraseId)),
        text(std::move(text)),
        language(std::move(language)),
        isTentativePhrase(isTentative)
    {}

    std::string text;
    std::string language;
    bool isTentativePhrase;
};

/**
* Represents speech.fragment message
*/
struct SpeechFragmentMsg : public SpeechMsg
{
    SpeechFragmentMsg(
        std::string&& content,
        OffsetType offset,
        DurationType duration,
        std::string&& text,
        std::string&& speaker = "",
        std::string&& id = "",
        std::string language="",
        std::string phraseId="") :
        SpeechMsg(std::move(content), offset, duration, std::move(speaker), std::move(id), std::move(phraseId)),
        text(std::move(text)),
        language(std::move(language))
    {}

    std::string text;
    std::string language;
};

/**
* Represents speech.keyword message
*/
struct SpeechKeywordDetectedMsg : public SpeechMsg
{
    SpeechKeywordDetectedMsg(std::string&& content, OffsetType offset, DurationType duration, KeywordVerificationStatus status, std::string&& text) :
        SpeechMsg(std::move(content), offset, duration),
        status(status),
        text(std::move(text))
    {}

    KeywordVerificationStatus status;
    std::string text;
};

/**
 * Represents speech.phrase message
 */
struct SpeechPhraseMsg : public SpeechMsg
{
    SpeechPhraseMsg() = default;

    SpeechPhraseMsg(std::string&& content, OffsetType offset, DurationType duration, RecognitionStatus status, std::string&& text, std::string&& speaker = "") :
        SpeechMsg(std::move(content), offset, duration, std::move(speaker)),
        recognitionStatus(status),
        displayText(std::move(text))
    {}

    RecognitionStatus recognitionStatus { RecognitionStatus::Error };
    std::string displayText;
    std::string language;
    ConfidenceLevel languageDetectionConfidence{ ConfidenceLevel::InvalidMessage };

    // Inline commit: token echoed from audio.commit via
    // speech.phrase.clientAudioMetadata["X-Client-Commit-Token"].
    // Zero means "no commit token present" (ordinary speech.phrase).
    uint32_t commitToken{ 0 };
};

/**
* Represents translation results.
*/
struct TranslationResult
{
    TranslationStatus translationStatus { TranslationStatus::Error };
    // A string indicates failure reasons in case that the translationStatus is an error.
    std::wstring failureReason;
    // An array of value pair <targetLanguage, translationText>.
    std::vector<std::tuple<std::string, std::string>> translations;
};
/**
* Represents translation.hypothesis message
*/
struct TranslationHypothesisMsg : public SpeechHypothesisMsg
{
    TranslationHypothesisMsg(
        std::string&& content,
        OffsetType offset,
        DurationType duration,
        std::string&& text,
        TranslationResult&& translation,
        std::string&& language = "") :
        SpeechHypothesisMsg(std::move(content), offset, duration, std::move(text), "", "", std::move(language)),
        translation(translation)
    {}

    TranslationResult translation;
};

/**
* Represents translation.phrase message
*/
struct TranslationPhraseMsg : public TranslationHypothesisMsg
{
    TranslationPhraseMsg(
        std::string&& content,
        OffsetType offset,
        DurationType duration,
        std::string&& text,
        TranslationResult&& translation,
        RecognitionStatus status,
        std::string&& language = "",
        ConfidenceLevel confidence = ConfidenceLevel::InvalidMessage,
        uint32_t commitTokenValue = 0) :
        TranslationHypothesisMsg(std::move(content), offset, duration, std::move(text), std::move(translation), std::move(language)),
        recognitionStatus(status),
        languageDetectionConfidence(confidence),
        commitToken(commitTokenValue)
    {}

    RecognitionStatus recognitionStatus;
    ConfidenceLevel languageDetectionConfidence;

    // Inline commit: token echoed from audio.commit via the nested
    // SpeechPhrase.clientAudioMetadata["X-Client-Commit-Token"] of a
    // translation.response message.
    // Zero means "no commit token present" (ordinary translation result).
    uint32_t commitToken{ 0 };
};

/**
* Represents an audio output chunk message
*/
struct AudioOutputChunkMsg
{
    std::string language;
    std::string requestId;
    const uint8_t* audioBuffer { nullptr };
    size_t audioLength { 0 };
};

/**
* Represents a text boundary for alignment with audio output
*/
struct TextBoundary
{
    OffsetType duration { 0 };
    std::string text;
    std::string boundaryType;
};

/**
* Represents a viseme event for speech synthesis
*/
struct Viseme
{
    OffsetType audioOffset { 0 };
    uint32_t visemeId { 0 };
    std::string animationChunk;
    bool isLastAnimation{ true };
};

/**
 * Represents a bookmark event for speech synthesis
 */
struct Bookmark
{
    OffsetType audioOffset{ 0 };
    std::string text;
};

/**
* Represents an audio output metadata
*/
struct AudioOutputMetadata
{
    std::string type;
    OffsetType audioOffset{ 0 };
    TextBoundary textBoundary;
    Viseme viseme;
    Bookmark bookmark;
};

/**
* Represents an audio output metadata message
*/
struct AudioOutputMetadataMsg
{
    std::string requestId;
    size_t size;
    std::list<AudioOutputMetadata> metadatas;
};

/**
* Represents a message corresponding to a user defined path.
*/
struct UserMsg
{
    const std::string path;
    const std::string contentType;
    const std::string requestId;
    const uint8_t* buffer;
    size_t size;
};

/**
* Represents a raw USP message
*/
struct RawMsg
{
    const std::string headers;
    const std::string path;
    const uint8_t* buffer;
    uint32_t bufferSize;
    bool isBufferBinary;
};

}}}}
