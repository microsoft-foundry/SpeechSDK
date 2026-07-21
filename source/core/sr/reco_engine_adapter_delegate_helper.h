//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "interface_delegate_helpers.h"
#include "ispxinterfaces.h"
#include "audio_processor_delegate_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxRecoEngineAdapter>>
class CSpxRecoEngineAdapterDelegateHelper :
    public DelegateToHelperT
{
private:

    using I = ISpxRecoEngineAdapter;
    using C = CSpxRecoEngineAdapterDelegateHelper<DelegateToHelperT>;

public:

    SPX_DELEGATE_ACCESSORS(RecoEngineAdapter, DelegateToHelperT, ISpxRecoEngineAdapter)

    void DelegateSetAdapterMode(bool singleShot)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SetAdapterMode, singleShot);
    }

    void DelegateSetKeyword(const std::string& keyword)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SetKeyword, keyword);
    }

    void DelegateOpenConnection(bool continuous)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::OpenConnection, continuous);
    }

    void DelegateCloseConnection()
    {
        InvokeOnDelegate(C::GetDelegate(), &I::CloseConnection);
    }

    void DelegateSendAgentMessage(const std::string& message)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SendAgentMessage, message);
    }

    void DelegateWriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::WriteTelemetryLatency, latencyInTicks, isPhraseLatency, isFirstHypothesisLatency);
    }

    void DelegateFlushTelemetry()
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FlushTelemetry);
    }

    void DelegateSendSpeechEventMessage(std::string&& message)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SendSpeechEventMessage, std::move(message));
    }

    void DelegateSendNetworkMessage(const char* path, std::string&& payload, const std::shared_ptr<std::promise<bool>>& completion)
    {
        InvokeOnDelegate(C::GetDelegate(), static_cast<void (I::*)(const char*, std::string &&, const std::shared_ptr<std::promise<bool>>&)>(&I::SendNetworkMessage), path, std::move(payload), completion);
    }

    void DelegateSendNetworkMessage(const char* path, std::vector<uint8_t>&& msg, const std::shared_ptr<std::promise<bool>>& completion)
    {
        InvokeOnDelegate(C::GetDelegate(), static_cast<void (I::*)(const char*, std::vector<uint8_t>&&, const std::shared_ptr<std::promise<bool>>&)>(&I::SendNetworkMessage), path, std::move(msg), completion);
    }

    void DelegateSetFormat(const SPXWAVEFORMATEX* pformat)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SetFormat, pformat);
    }

    void DelegateProcessAudio(const DataChunkPtr& audioChunk)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::ProcessAudio, audioChunk);
    }

};
}}}} // Microsoft::CognitiveServices::Speech::Impl
