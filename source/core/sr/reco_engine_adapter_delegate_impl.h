//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "reco_engine_adapter_delegate_helper.h"
#include "audio_processor_delegate_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <typename DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxRecoEngineAdapter>>
class ISpxRecoEngineAdapterDelegateImpl :
    public CSpxRecoEngineAdapterDelegateHelper<DelegateToHelperT>,
    public ISpxRecoEngineAdapter
{
private:

    using D = CSpxRecoEngineAdapterDelegateHelper<DelegateToHelperT>;

public:

    void SetAdapterMode(bool singleShot) override
    {
        D::DelegateSetAdapterMode(singleShot);
    }

    void OpenConnection(bool singleShot) override
    {
        D::DelegateOpenConnection(singleShot);
    }

    void CloseConnection() override
    {
        D::DelegateCloseConnection();
    }

    void WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency) override
    {
        D::DelegateWriteTelemetryLatency(latencyInTicks, isPhraseLatency, isFirstHypothesisLatency);
    }

    void FlushTelemetry() override
    {
        D::DelegateFlushTelemetry();
    }

    void SendAgentMessage(const std::string& buffer) override
    {
        D::DelegateSendAgentMessage(buffer);
    }

    void SendSpeechEventMessage(std::string&& msg) override
    {
        D::DelegateSendSpeechEventMessage(std::move(msg));
    }

    void SendNetworkMessage(const char* path, std::string&& msg, const std::shared_ptr<std::promise<bool>>& pr) override
    {
        D::DelegateSendNetworkMessage(path, std::move(msg), pr);
    }

    void SendNetworkMessage(const char* path, std::vector<uint8_t>&& msg, const std::shared_ptr<std::promise<bool>>& pr) override
    {
        D::DelegateSendNetworkMessage(path, std::move(msg), pr);
    }

    void SetFormat(const SPXWAVEFORMATEX* pformat) override
    {
        D::DelegateSetFormat(pformat);
    }

    void ProcessAudio(const DataChunkPtr& audioChunk) override
    {
        D::DelegateProcessAudio(audioChunk);
    }
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
