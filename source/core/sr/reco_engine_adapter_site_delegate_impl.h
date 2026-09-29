//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "interfaces/base.h"
#include "reco_engine_adapter_site_delegate_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <typename DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxRecoEngineAdapterSite>>
class ISpxRecoEngineAdapterSiteDelegateImpl :
    public CSpxRecoEngineAdapterSiteDelegateHelper<DelegateToHelperT>,
    public ISpxRecoEngineAdapterSite
{
private:

    using D = CSpxRecoEngineAdapterSiteDelegateHelper<DelegateToHelperT>;

public:
    void GetScenarioCount(uint16_t* countSpeech, uint16_t* countTranslation, uint16_t* countDialog, uint16_t* countConversationTranscriber, uint16_t* countConversationTranscriberV2, uint16_t* countMeetingTranscriber, uint16_t* countLanguageId) override
    {
        D::DelegateGetScenarioCount(countSpeech, countTranslation, countDialog, countConversationTranscriber, countConversationTranscriberV2, countMeetingTranscriber, countLanguageId);
    }

    std::list<std::string> GetListenForList() override
    {
        return D::DelegateGetListenForList();
    }

    std::shared_ptr<ISpxRecognitionResult> GetSpottedKeywordResult() override
    {
        return D::DelegateGetSpottedKeywordResult();
    }

    void AdapterStartingTurn(ISpxRecoEngineAdapter* adapter) override
    {
        D::DelegateAdapterStartingTurn(adapter);
    }

    void AdapterStartedTurn(ISpxRecoEngineAdapter* adapter, const std::string& id, OffsetType adapterStartOffset) override
    {
        D::DelegateAdapterStartedTurn(adapter, id, adapterStartOffset);
    }

    void AdapterStoppedTurn(ISpxRecoEngineAdapter* adapter, bool isRestarting) override
    {
        D::DelegateAdapterStoppedTurn(adapter, isRestarting);
    }

    bool IsExpectingAdapterStoppedTurn(ISpxRecoEngineAdapter* adapter) override
    {
        return D::DelegateIsExpectingAdapterStoppedTurn(adapter);
    }

    void AdapterDetectedSpeechStart(ISpxRecoEngineAdapter* adapter, uint64_t offset) override
    {
        D::DelegateAdapterDetectedSpeechStart(adapter, offset);
    }

    void AdapterDetectedSpeechEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset) override
    {
        D::DelegateAdapterDetectedSpeechEnd(adapter, offset);
    }

    void AdapterDetectedSoundStart(ISpxRecoEngineAdapter* adapter, uint64_t offset) override
    {
        D::DelegateAdapterDetectedSoundStart(adapter, offset);
    }

    void AdapterDetectedSoundEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset) override
    {
        D::DelegateAdapterDetectedSoundEnd(adapter, offset);
    }

    void AdapterEndOfDictation(ISpxRecoEngineAdapter* adapter, uint64_t offset, uint64_t duration) override
    {
        D::DelegateAdapterEndOfDictation(adapter, offset, duration);
    }

    void FireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override
    {
        D::DelegateFireAdapterResult_Intermediate(offset, result);
    }

    void FireAdapterResult_KeywordResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isAccepted) override
    {
        D::DelegateFireAdapterResult_KeywordResult(offset, result, isAccepted);
    }

    void FireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override
    {
        D::DelegateFireAdapterResult_FinalResult(offset, result);
    }

    void FireAdapterResult_ActivityReceived(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) override
    {
        D::DelegateFireAdapterResult_ActivityReceived(activity, audio);
    }

    void FireAdapterResult_TurnStatusReceived(std::wstring interactionId, std::string conversationId, int statusCode) override
    {
        D::DelegateFireAdapterResult_TurnStatusReceived(interactionId, conversationId, statusCode);
    }

    void FireAdapterResult_TranslationSynthesis(std::shared_ptr<ISpxRecognitionResult> result) override
    {
        D::DelegateFireAdapterResult_TranslationSynthesis(result);
    }

    void AdapterConnected(const std::string& url) override
    {
        D::DelegateAdapterConnected(url);
    }

    void AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload) override
    {
        D::DelegateAdapterDisconnected(payload);
    }

    void FireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary) override
    {
        D::DelegateFireConnectionMessageReceived(headers, path, buffer, bufferSize, isBufferBinary);
    }

    void AdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* adapter) override
    {
        D::DelegateAdapterCompletedSetFormatStop(adapter);
    }

    void AdapterCommitAcknowledged(ISpxRecoEngineAdapter* adapter, uint32_t token, uint64_t offset, uint64_t duration) override
    {
        D::DelegateAdapterCommitAcknowledged(adapter, token, offset, duration);
    }

    void AdapterRequestingAudioMute(ISpxRecoEngineAdapter* adapter, bool muteAudio) override
    {
        D::DelegateAdapterRequestingAudioMute(adapter, muteAudio);
    }

    void AdditionalMessage(ISpxRecoEngineAdapter* adapter, uint64_t offset, AdditionalMessagePayload_Type payload) override
    {
        D::DelegateAdditionalMessage(adapter, offset, payload);
    }

    void Error(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload) override
    {
        D::DelegateError(adapter, payload);
    }
};

template <class T>
class ISpxRecoEngineAdapterSiteDelegateToSiteImpl : public ISpxRecoEngineAdapterSiteDelegateImpl<CSpxDelegateToSiteWeakPtrHelper<ISpxRecoEngineAdapterSite, T>>
{
};

}
}
}
}
