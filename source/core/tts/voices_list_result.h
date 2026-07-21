//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// voices_list_result.h: Implementation declarations for CSpxSynthesisVoicesResult C++ class
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSynthesisVoicesResult :
    public ISpxSynthesisVoicesResult,
    public ISpxSynthesisVoicesResultInit,
    public ISpxPropertyBagImpl
{
public:

    CSpxSynthesisVoicesResult();
    ~CSpxSynthesisVoicesResult();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxSynthesisVoicesResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxSynthesisVoicesResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // --- ISpxSynthesisVoicesResult ---
    std::shared_ptr<std::vector<std::shared_ptr<ISpxVoiceInfo>>> GetVoices() override;
    std::string GetResultId() override;
    ResultReason GetReason() override;
    const std::shared_ptr<ISpxErrorInformation>& GetError() override;

    // --- ISpxSynthesisVoicesResultInit ---
    void InitSuccessResult(const std::string& resultId) override;
    void InitErrorResult(const std::shared_ptr<ISpxErrorInformation>& error, const std::string& resultId) override;
    void AppendVoice(const std::shared_ptr<ISpxVoiceInfo>& voice) override;

    // --- ISpxNamedProperties (overrides)
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;

private:

    DISABLE_COPY_AND_MOVE(CSpxSynthesisVoicesResult);

    std::string m_resultId;
    std::shared_ptr<std::vector<std::shared_ptr<ISpxVoiceInfo>>> m_voices;
    ResultReason m_reason;
    std::shared_ptr<ISpxErrorInformation> m_error;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
