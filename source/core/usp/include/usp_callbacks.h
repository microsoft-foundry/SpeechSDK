//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "uspmessages.h"
#include "usp_message.h"
#include "site_helpers.h"
#include "interface_helpers.h"
#include "ispxinterfaces.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

/**
* The Callbacks type represents an application-defined structure used to register callbacks for USP events.
* The callbacks are invoked during the processing of the request, an application should spend as little time as possible
* in the callback function.
*/
struct Callbacks
{
    /**
     * A callback function that will be invoked for all USP messages received from the service.
    */
    virtual void OnMessageReceived(const RawMsg&) {}

    /**
     * A callback function that will be invoked when a speech.startDetected message is received from service.
    */
    virtual void OnSpeechStartDetected(const SpeechStartDetectedMsg&) {}

    /**
    * A callback function that will be invoked when a speech.endDetected message is received from service.
    */
    virtual void OnSpeechEndDetected(const SpeechEndDetectedMsg&) {}

    /**
    * A callback function that will be invoked when a speech.hypothesis message is received from service.
    */
    virtual void OnSpeechHypothesis(const SpeechHypothesisMsg&) {}

    /**
    * A callback function that will be invoked when a speech.keyword message is received from service.
    */
    virtual void OnSpeechKeywordDetected(const SpeechKeywordDetectedMsg&) {}

    /**
    * A callback function that will be invoked when a speech.phrase message is received from service.
    */
    virtual void OnSpeechPhrase(const SpeechPhraseMsg&) {}

    /**
    * A callback function that will be invoked when a speech.fragment message is received from service.
    */
    virtual void OnSpeechFragment(const SpeechFragmentMsg&) {}

    /**
    * A callback function that will be invoked when a turn.start message is received from service.
    */
    virtual void OnTurnStart(const TurnStartMsg&) {}

    /**
    * A callback function that will be invoked when a turn.end message is received from service.
    */
    virtual void OnTurnEnd(const TurnEndMsg&) {}

    /**
    * A callback function that will be invoked when a turn.start unrelated to speech is received from service.
    */
    virtual void OnMessageStart(const TurnStartMsg&) {}

    /**
    * A callback function that will be invoked when a turn.end message unrelated to speech is received from service.
    */
    virtual void OnMessageEnd(const TurnEndMsg&) {}

    /**
    * A callback function that will be invoked when an error occurs in handling communication with service.
    */
    virtual void OnError(const std::shared_ptr<Impl::ISpxErrorInformation>&) {}

    /**
    * A callback function that will be invoked when a translation.hypothesis message is received from service.
    */
    virtual void OnTranslationHypothesis(const TranslationHypothesisMsg&) {}

    /**
    * A callback function that will be invoked when a translation.phrase message is received from service.
    */
    virtual void OnTranslationPhrase(const TranslationPhraseMsg&) {}

    /**
    * A callback function that will be invoked when an audio output chunk message is received from service.
    */
    virtual void OnAudioOutputChunk(const AudioOutputChunkMsg&) {}

    /**
    * A callback function that will be invoked when an audio output metadata message is received from service.
    */
    virtual void OnAudioOutputMetadata(const AudioOutputMetadataMsg&) {}

    /**
    * A callback function that will be invoked when a message having a path defined by user is received from service.
    */
    virtual void OnUserMessage(const UserMsg&) {}

    /**
     * A callback function that will be invoked when the connection to service is established.
    */
    virtual void OnConnected(const std::string&) {}

    /**
    * A callback function that will be invoked when the connection to service is lost.
    */
    virtual void OnDisconnected(const std::shared_ptr<ISpxErrorInformation>&) {}

    virtual void OnToken(const std::string /*token*/) {}

    virtual void OnAcknowledgedAudio(uint64_t /*offset*/) {}
};

class ISpxUspCallbacks :
    public Impl::ISpxInterfaceBaseFor<ISpxUspCallbacks>,
    public Callbacks
{
};

class CSpxUspCallbackWrapper final :
    public Impl::ISpxObjectWithSiteInitImpl<ISpxUspCallbacks>,
    public ISpxUspCallbacks
{
public:

    CSpxUspCallbackWrapper() = default;
    ~CSpxUspCallbackWrapper() = default;

    SPX_INTERFACE_MAP_BEGIN()
        using namespace Impl;
    SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxUspCallbacks)
    SPX_INTERFACE_MAP_END()

    // --- ISpxUspCallbacks (overrides)
    inline void OnMessageReceived(const USP::RawMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnMessageReceived(m); }); }
    inline void OnSpeechStartDetected(const USP::SpeechStartDetectedMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnSpeechStartDetected(m); }); }
    inline void OnSpeechEndDetected(const USP::SpeechEndDetectedMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnSpeechEndDetected(m); }); }
    inline void OnSpeechHypothesis(const USP::SpeechHypothesisMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnSpeechHypothesis(m); }); }
    inline void OnSpeechKeywordDetected(const USP::SpeechKeywordDetectedMsg& m) override { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnSpeechKeywordDetected(m); }); }
    inline void OnSpeechPhrase(const USP::SpeechPhraseMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnSpeechPhrase(m); }); }
    inline void OnSpeechFragment(const USP::SpeechFragmentMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnSpeechFragment(m); }); }
    inline void OnTurnStart(const USP::TurnStartMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnTurnStart(m); }); }
    inline void OnTurnEnd(const USP::TurnEndMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnTurnEnd(m); }); }
    inline void OnMessageStart(const USP::TurnStartMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnMessageStart(m); }); }
    inline void OnMessageEnd(const USP::TurnEndMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnMessageEnd(m); }); }
    inline void OnError(const std::shared_ptr<ISpxErrorInformation>& error) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnError(error); }); }
    inline void OnTranslationHypothesis(const USP::TranslationHypothesisMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnTranslationHypothesis(m); }); }
    inline void OnTranslationPhrase(const USP::TranslationPhraseMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnTranslationPhrase(m); }); }
    inline void OnAudioOutputChunk(const USP::AudioOutputChunkMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnAudioOutputChunk(m); }); }
    inline void OnAudioOutputMetadata(const USP::AudioOutputMetadataMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnAudioOutputMetadata(m); }); }
    inline void OnUserMessage(const USP::UserMsg& m) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnUserMessage(m); }); }
    inline void OnConnected(const std::string& url) final { InvokeOnSite([&](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnConnected(url); }); }
    inline void OnDisconnected(const std::shared_ptr<ISpxErrorInformation>& error) final { InvokeOnSite([&error](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnDisconnected(error); }); }
    inline void OnToken(const std::string s) final { InvokeOnSite([s](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnToken(s); }); }
    inline void OnAcknowledgedAudio(uint64_t s) final { InvokeOnSite([s](std::shared_ptr<ISpxUspCallbacks> callback) { callback->OnAcknowledgedAudio(s); }); }

private:
    DISABLE_COPY_AND_MOVE(CSpxUspCallbackWrapper);
};

using CallbacksPtr = std::shared_ptr<Callbacks>;

}}}}
