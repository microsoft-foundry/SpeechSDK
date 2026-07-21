//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <ispxinterfaces.h>
#include <interface_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSynthesizerConnection :
    public ISpxSynthesizerConnection,
    public ISpxConnection,
    public ISpxMessageParamFromUser
{
public:
    CSpxSynthesizerConnection();
    virtual ~CSpxSynthesizerConnection();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxSynthesizerConnection)
        SPX_INTERFACE_MAP_ENTRY(ISpxConnection)
        SPX_INTERFACE_MAP_ENTRY(ISpxMessageParamFromUser)
    SPX_INTERFACE_MAP_END()

    // --- ISpxSynthesizerConnection ---
    void Init(std::weak_ptr<ISpxSynthesizer> synthesizer) override;

    // --- ISpxConnection ---
    void Open(bool forContinuousRecognition) override;
    void Close() override;
    std::shared_ptr<ISpxRecognizer> GetRecognizer() override;
    std::shared_ptr<ISpxSynthesizer> GetSynthesizer() override;

    // --- ISpxUspMessageParamFromUser ---
    void SetParameter(const char *path, const char *name, const char *value) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::string&& payload) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload) override;

private:
    DISABLE_COPY_AND_MOVE(CSpxSynthesizerConnection);

    std::shared_ptr<ISpxTtsEngineAdapter> GetTtsEngineAdapter();

private:
    std::weak_ptr<ISpxSynthesizer> m_synthesizer;
    std::weak_ptr<ISpxMessageParamFromUser> m_setMessageParamFromUser;
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
