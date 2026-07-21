//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <string>
#include <queue>
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSynthesisRequest :
    public ISpxSynthesisRequest,
    public ISpxSynthesisRequestReader,
    public ISpxPropertyBagImpl
{
public:

    CSpxSynthesisRequest();
    ~CSpxSynthesisRequest();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxSynthesisRequest)
        SPX_INTERFACE_MAP_ENTRY(ISpxSynthesisRequestReader)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // --- ISpxSynthesisRequest ---
    void Init(SynthesisRequestInputType inputType, const std::string& inputContent, const std::string& requestId) override;
    void SetVoiceName(const std::string& voiceName, SynthesisRequestVoiceType voiceType, const std::string& modelName) override;
    void SetRequestId(const std::string& requestId) override;
    void SendTextPiece(const std::string& textPiece) override;
    void FinishInput() override;

    // --- ISpxSynthesisRequestReader ---
    SynthesisRequestInputType GetInputType() const override;
    std::tuple<std::string, SynthesisRequestVoiceType, std::string> GetVoiceName() override;
    std::string& GetInputContent() override;
    std::string& GetRequestId() override;
    std::tuple<bool, std::string> GetNextTextPiece() override;


private:
    std::string m_requestId;
    std::string m_inputContent;
    SynthesisRequestInputType m_inputType;

    std::string m_voiceName;
    SynthesisRequestVoiceType m_voiceType { SynthesisRequestVoiceType::None };
    std::string m_modelName;

    std::queue<std::string> m_textPieces;
    std::mutex m_textPiecesMutex;
    std::condition_variable m_textPiecesCV;
    bool m_inputFinished;

    bool m_initialized;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl