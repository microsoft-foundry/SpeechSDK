//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <string>

#include <interfaces/base.h>
#include <interfaces/results.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxSessionEventArgs)
{
    public:
    virtual const std::wstring& GetSessionId() = 0;
};

SPX_INTERFACE(ISpxSessionEventArgsInit)
{
    public:
    virtual void Init(const std::wstring& sessionId) = 0;
};

SPX_INTERFACE(ISpxRecognitionEventArgs)
{
    public:
    virtual const uint64_t& GetOffset() = 0;
    virtual std::shared_ptr<ISpxRecognitionResult> GetResult() = 0;
};

SPX_INTERFACE(ISpxRecognitionEventArgsInit)
{
    public:
    virtual void Init(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result) = 0;
    virtual void Init(const std::wstring& sessionId, uint64_t offset) = 0;
};

SPX_INTERFACE(ISpxConnectionEventArgs)
{
};

SPX_INTERFACE(ISpxConnectionEventArgsInit)
{
    public:
    virtual void Init(const std::wstring& sessionId) = 0;
};


/// <summary>
/// Interface for miscellaneous conversation related event arguments (e.g. expiration, participants
/// changed, etc...)
/// </summary>
SPX_INTERFACE(ISpxConversationEventArgs)
{
};

/// <summary>
/// Interface speech synthesis metadata event arguments. (e.g., WordBoundary, Viseme, etc...)
/// </summary>
SPX_INTERFACE(ISpxSpeechSynthesisMetadataEventArgs)
{
    public:
    virtual std::string& GetResultId() = 0;
};

SPX_INTERFACE(ISpxSpeechSynthesisMetadataEventArgsInit)
{
    public:
    virtual void SetResultId(std::string resultId) = 0;
};

SPX_INTERFACE(ISpxWordBoundaryEventArgs)
{
    public:
    virtual uint64_t GetAudioOffset() = 0;
    virtual uint64_t GetDuration() = 0;
    virtual uint32_t GetTextOffset() = 0;
    virtual uint32_t GetWordLength() = 0;
    virtual std::string& GetText() = 0;
    virtual SpeechSynthesisBoundaryType GetBoundaryType() = 0;
};

SPX_INTERFACE(ISpxWordBoundaryEventArgsInit)
{
    public:
        virtual void Init(uint64_t audioOffset, uint64_t duration, uint32_t textOffset,
                          uint32_t wordLength, std::string text, SpeechSynthesisBoundaryType boundaryType) = 0;
};

SPX_INTERFACE(ISpxVisemeEventArgs)
{
    public:
    virtual uint64_t GetAudioOffset() = 0;
    virtual uint32_t GetVisemeId() = 0;
    virtual std::string& GetAnimation() = 0;
};

SPX_INTERFACE(ISpxVisemeEventArgsInit)
{
    public:
    virtual void Init(uint64_t audioOffset, uint32_t visemeId, std::string animation) = 0;
};

SPX_INTERFACE(ISpxBookmarkEventArgs)
{
    public:
    virtual uint64_t GetAudioOffset() = 0;
    virtual std::string& GetText() = 0;
};

SPX_INTERFACE(ISpxBookmarkEventArgsInit)
{
    public:
    virtual void Init(uint64_t audioOffset, std::string text) = 0;
};

} } } }
