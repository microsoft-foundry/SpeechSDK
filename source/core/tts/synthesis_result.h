//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// synthesis_result.h: Implementation declarations for CSpxSynthesisResult C++ class
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSynthesisResult :
    public ISpxSynthesisResult,
    public ISpxSynthesisResultInit,
    public ISpxPropertyBagImpl
{
public:

    CSpxSynthesisResult();
    ~CSpxSynthesisResult();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxSynthesisResult)
        SPX_INTERFACE_MAP_ENTRY(ISpxSynthesisResultInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // --- ISpxSynthesisResult ---
    std::string GetResultId() override;
    std::string GetRequestId() override;
    ResultReason GetReason() override;
    CancellationReason GetCancellationReason() override;
    const std::shared_ptr<ISpxErrorInformation>& GetError() override;
    uint32_t GetAudioLength() override;
    uint64_t GetAudioDuration() override;
    std::shared_ptr<std::vector<uint8_t>> GetAudioData() override;
    std::shared_ptr<std::vector<uint8_t>> GetRawAudioData() override;
    std::shared_ptr<ISpxAudioDataStream> GetAudioDataStream() override;
    SpxWAVEFORMATEX_Type GetFormat() override;
    bool HasHeader() override;

    // --- ISpxSynthesisResultInit ---
    void InitSynthesisResult(const std::string& requestId, ResultReason reason,
        const std::shared_ptr<ISpxErrorInformation>& error) override;
    void SetAudioData(std::shared_ptr<std::vector<uint8_t>> audio, uint64_t duration) override;
    void SetAudioFormat(std::shared_ptr<SPXWAVEFORMATEX> format, bool hasHeader) override;
    void SetAudioDataStream(const std::shared_ptr<ISpxAudioDataStream>& stream) override;
    void UpdateError(const std::shared_ptr<ISpxErrorInformation>& error) override;
    void Reset() override;

    // --- ISpxNamedProperties (overrides) ---
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;

protected:

    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override;

private:

    DISABLE_COPY_AND_MOVE(CSpxSynthesisResult);

    std::string m_requestId;
    std::shared_ptr<CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>> m_futureResult;
    ResultReason m_reason;
    CancellationReason m_cancellationReason;
    std::shared_ptr<ISpxErrorInformation> m_error;
    std::shared_ptr<std::vector<uint8_t>> m_audioData;
    std::shared_ptr<std::vector<uint8_t>> m_rawAudioData;
    SpxWAVEFORMATEX_Type m_audioFormat;
    bool m_hasHeader = true;
    size_t m_headerLength = 0;
    uint64_t m_audioDuration = 0;

    std::shared_ptr<ISpxAudioDataStream> m_audioStream;
    std::shared_ptr<ISpxAudioDataStream> m_coreStream;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
