//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "interface_delegate_helpers.h"
#include "ispxinterfaces.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxRecoEngineAdapterSite>>
class CSpxRecoEngineAdapterSiteDelegateHelper : public DelegateToHelperT
{
private:

    using I = ISpxRecoEngineAdapterSite;
    using C = CSpxRecoEngineAdapterSiteDelegateHelper<DelegateToHelperT>;

public:

    SPX_DELEGATE_ACCESSORS(RecoEngineAdapterSite, DelegateToHelperT, ISpxRecoEngineAdapterSite)

    void DelegateGetScenarioCount(uint16_t* countSpeech, uint16_t* countTranslation, uint16_t* countDialog, uint16_t* countConversationTranscriber, uint16_t* countConversationTranscriberV2, uint16_t* countMeetingTranscriber, uint16_t* countLanguageId)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::GetScenarioCount, countSpeech, countTranslation, countDialog, countConversationTranscriber, countConversationTranscriberV2, countMeetingTranscriber, countLanguageId);
    }

    std::list<std::string> DelegateGetListenForList()
    {
        return InvokeOnDelegateR(C::GetDelegate(), &I::GetListenForList, std::list<std::string>());
    }

    std::shared_ptr<ISpxRecognitionResult> DelegateGetSpottedKeywordResult()
    {
        return InvokeOnDelegateR(C::GetDelegate(), &I::GetSpottedKeywordResult, nullptr);
    }

    void DelegateAdapterStartingTurn(ISpxRecoEngineAdapter* adapter)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterStartingTurn, adapter);
    }

    void DelegateAdapterStartedTurn(ISpxRecoEngineAdapter* adapter, const std::string& id, OffsetType adapterStartOffset)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterStartedTurn, adapter, id, adapterStartOffset);
    }

    void DelegateAdapterStoppedTurn(ISpxRecoEngineAdapter* adapter, bool isRestarting)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterStoppedTurn, adapter, isRestarting);
    }

    bool DelegateIsExpectingAdapterStoppedTurn(ISpxRecoEngineAdapter* adapter)
    {
        return InvokeOnDelegateR(C::GetDelegate(), &I::IsExpectingAdapterStoppedTurn, false, adapter);
    }

    void DelegateAdapterDetectedSpeechStart(ISpxRecoEngineAdapter* adapter, uint64_t offset)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterDetectedSpeechStart, adapter, offset);
    }

    void DelegateAdapterDetectedSpeechEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterDetectedSpeechEnd, adapter, offset);
    }

    void DelegateAdapterDetectedSoundStart(ISpxRecoEngineAdapter* adapter, uint64_t offset)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterDetectedSoundStart, adapter, offset);
    }

    void DelegateAdapterDetectedSoundEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterDetectedSoundEnd, adapter, offset);
    }

    void DelegateAdapterEndOfDictation(ISpxRecoEngineAdapter* adapter, uint64_t offset, uint64_t duration)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterEndOfDictation, adapter, offset, duration);
    }

    void DelegateFireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FireAdapterResult_Intermediate, offset, result);
    }

    void DelegateFireAdapterResult_KeywordResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isAccepted)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FireAdapterResult_KeywordResult, offset, result, isAccepted);
    }

    void DelegateFireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FireAdapterResult_FinalResult, offset, result);
    }

    void DelegateFireAdapterResult_ActivityReceived(std::string activity, std::shared_ptr<ISpxAudioOutput> audio)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FireAdapterResult_ActivityReceived, activity, audio);
    }

    void DelegateFireAdapterResult_TurnStatusReceived(std::wstring interactionId, std::string conversationId, int statusCode)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FireAdapterResult_TurnStatusReceived, interactionId, conversationId, statusCode);
    }

    void DelegateFireAdapterResult_TranslationSynthesis(std::shared_ptr<ISpxRecognitionResult> result)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FireAdapterResult_TranslationSynthesis, result);
    }

    void DelegateAdapterConnected(const std::string& url)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterConnected, url);
    }

    void DelegateAdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterDisconnected, payload);
    }

    void DelegateFireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::FireConnectionMessageReceived, headers, path, buffer, bufferSize, isBufferBinary);
    }

    void DelegateAdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* adapter)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterCompletedSetFormatStop, adapter);
    }

    void DelegateAdapterCommitAcknowledged(ISpxRecoEngineAdapter* adapter, uint32_t token, uint64_t offset, uint64_t duration)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterCommitAcknowledged, adapter, token, offset, duration);
    }

    void DelegateAdapterRequestingAudioMute(ISpxRecoEngineAdapter* adapter, bool muteAudio)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdapterRequestingAudioMute, adapter, muteAudio);
    }

    void DelegateAdditionalMessage(ISpxRecoEngineAdapter* adapter, uint64_t offset, ISpxRecoEngineAdapterSite::AdditionalMessagePayload_Type payload)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::AdditionalMessage, adapter, offset, payload);
    }

    void DelegateError(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::Error, adapter, payload);
    }
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
