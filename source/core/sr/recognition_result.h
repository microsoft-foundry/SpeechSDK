//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <string>

#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxRecognitionResult :
    public ISpxRecognitionResult,
    public ISpxRecognitionResultInit,
    public ISpxKeywordRecognitionResult,
    public ISpxKeywordRecognitionResultInit,
    public ISpxConversationTranscriptionResult,
    public ISpxConversationTranscriptionV2Result,
    public ISpxMeetingTranscriptionResult,
    public ISpxConversationTranscriptionResultInit,
    public ISpxConversationTranscriptionV2ResultInit,
    public ISpxMeetingTranscriptionResultInit,
    public ISpxTranslationRecognitionResult,
    public ISpxTranslationRecognitionResultInit,
    public ISpxTranslationSynthesisResult,
    public ISpxTranslationSynthesisResultInit,
    public ISpxPropertyBagImpl
{
public:

    CSpxRecognitionResult();
    virtual ~CSpxRecognitionResult();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxRecognitionResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecognitionResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxKeywordRecognitionResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxKeywordRecognitionResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxConversationTranscriptionResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxConversationTranscriptionV2Result)
        SPX_INTERFACE_MAP_ENTRY(ISpxConversationTranscriptionResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxConversationTranscriptionV2ResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxMeetingTranscriptionResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxMeetingTranscriptionResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxTranslationRecognitionResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxTranslationRecognitionResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxTranslationSynthesisResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxTranslationSynthesisResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // --- ISpxRecognitionResult ---
    std::string GetResultId() override;
    std::string GetText() override;

    ResultReason GetReason() override;
    NoMatchReason GetNoMatchReason() override;
    CancellationReason GetCancellationReason() override;
    std::shared_ptr<ISpxErrorInformation> GetError() override;

    uint64_t GetOffset() const override { return m_offset; }
    uint64_t GetDuration() const override { return m_duration; }
    void SetOffset(uint64_t offset) override { m_offset = offset; }

    // Inline commit: non-zero when this result acknowledges a commit
    // request (populated by CSpxAudioStreamSession on ACK arrival).
    uint32_t GetCommitToken() const override { return m_commitToken; }
    void SetCommitToken(uint32_t commitToken) override { m_commitToken = commitToken; }

    void SetLatency(uint64_t latency) override;

    inline std::shared_ptr<ISpxAudioDataStream> GetAudioDataStream() final
    {
        return m_stream;
    }

    // --- ISpxRecognitionResultInit ---
    void InitIntermediateResult(const char* text, uint64_t offset, uint64_t duration, const char* phraseId) override;
    void InitFinalResult(
        ResultReason reason,
        NoMatchReason noMatchReason,
        const char* text,
        uint64_t offset,
        uint64_t duration,
        const char* phraseId) override;
    void InitErrorResult(const std::shared_ptr<ISpxErrorInformation>& error) override;
    void InitEndOfStreamResult() override;

    // --- ISpxKeywordRecognitionResultInit ---
    void InitKeywordResult(const double confidence, const uint64_t offset, const uint64_t duration, const char* keyword, ResultReason reason, std::shared_ptr<ISpxAudioDataStream> stream) override;

    // --- ISpxKeywordRecognitionResult ---
    double GetConfidence() override;

    // --- ISpxConversationTranscriptionResult ---
    // --- ISpxMeetingTranscriptionResult ---
    std::string GetUserId() override;
    std::string GetUtteranceId() override;

    // --- ISpxConversationTranscriptionV2Result ---
    std::string GetSpeakerId() override;

    // --- ISpxConversationTranscriptionResultInit ---
    void InitConversationResult(const char* userId, const char* utteranceId) override;

    // --- ISpxConversationTranscriptionV2ResultInit ---
    void InitConversationV2Result(const char* speakerId) override;

    // --- ISpxMeetingTranscriptionResultInit ---
    void InitMeetingResult(const char* userId, const char* utteranceId) override;

    // --- ISpxTranslationRecognitionResult ---
    const std::vector<std::tuple<std::string, std::string>>& GetTranslationText() override;

    // --- ISpxTranslationRecognitionResultInit ---
    void InitTranslationRecognitionResult(TranslationStatusCode status, std::vector<std::tuple<std::string, std::string>> translations, const std::wstring& failureReason) override;

    // --- ISpxTranslationSynthesisResult ---
    const uint8_t* GetAudio() const override;
    size_t GetLength() const override;
    std::string GetRequestId() const override;

    // --- ISpxTranslationSynthesisResultInit ---
    void InitTranslationSynthesisResult(const uint8_t* audioData, size_t audioLength, const std::string& requestId) override;

    // --- ISpxNamedProperties (overrides) ---
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;

private:

    CSpxRecognitionResult(const CSpxRecognitionResult&) = delete;
    CSpxRecognitionResult(const CSpxRecognitionResult&&) = delete;

    CSpxRecognitionResult& operator=(const CSpxRecognitionResult&) = delete;

    std::string m_resultId;
    std::string m_text;
    ResultReason m_reason;
    CancellationReason m_cancellationReason;
    std::shared_ptr<ISpxErrorInformation> m_error;
    NoMatchReason m_noMatchReason;
    std::shared_ptr<ISpxAudioDataStream> m_stream;

    double m_confidence;

    std::string m_speakerId;
    std::string m_userId;
    std::string m_utteranceId;

    std::vector<std::tuple<std::string, std::string>> m_translations;
    std::string m_entitiesJson;

    std::vector<uint8_t> m_audioBuffer;
    size_t m_audioLength{0};
    uint64_t m_offset{0};
    uint64_t m_duration{0};
    uint32_t m_commitToken{0};
    std::string m_requestId;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
