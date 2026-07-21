//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "interfaces/types.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

enum class SynthesisRequestInputType
{
    None,
    Text,
    SSML,
    TextStream,
};

enum class SynthesisRequestVoiceType
{
    None,
    PREBUILT,
    CUSTOM,
    PERSONAL,
};

SPX_INTERFACE(ISpxSynthesisRequest)
{
    public:
    virtual void Init(SynthesisRequestInputType inputType, const std::string& inputContent, const std::string& requestId) = 0;

    virtual void SetVoiceName(const std::string& voiceName, SynthesisRequestVoiceType voiceType, const std::string& modelName) = 0;
    virtual void SetRequestId(const std::string& requestId) = 0;
    virtual void SendTextPiece(const std::string& textPiece) = 0;
    virtual void FinishInput() = 0;
};

SPX_INTERFACE(ISpxSynthesisRequestReader)
{
    public:
    virtual SynthesisRequestInputType GetInputType() const = 0;
    virtual std::tuple<std::string, SynthesisRequestVoiceType, std::string> GetVoiceName() = 0;
    virtual std::string& GetInputContent() = 0;
    virtual std::string& GetRequestId() = 0;
    virtual std::tuple<bool, std::string> GetNextTextPiece() = 0;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl