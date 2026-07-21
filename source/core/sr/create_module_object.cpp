//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_module_object.cpp: Implementation definitions for *CreateModuleObject* methods
//

#include "stdafx.h"

#include "activity_event_args.h"
#include "turn_status_event_args.h"
#include "audio_stream_session.h"
#include "factory_helpers.h"
#include "conversation_transcriber_v2.h"
#include "meeting_transcriber.h"
#include "meeting.h"
#include "translation_recognizer.h"
#include "keyword_spotter_model.h"
#include "phrase.h"
#include "phrase_list_grammar.h"
#include "recognition_event_args.h"
#include "recognition_result.h"
#include "recognizer.h"
#include "session_event_args.h"
#include "token_request_event_args.h"
#include "connection_event_args.h"
#include "connection_message.h"
#include "connection_message_event_args.h"
#include "usp_reco_engine_adapter.h"
#include "usp_reco_engine_adapter_retry.h"
#include "hybrid_reco_engine_adapter.h"
#include "reco_engine_adapter_offset_fixup_wrapper.h"
#include "dialog_service_connector.h"
#include "speech_audio_processor.h"
#include "connection.h"
#include "user.h"
#include "participant.h"
#include "stored_grammar.h"
#include "class_language_model.h"
#include "speech_config.h"
#include "speech_translation_config.h"
#include "auto_detect_source_lang_config.h"
#include "source_lang_config.h"
#include "audio_session_shim.h"
#include "output_reco_adapter.h"
#include "pronunciation_assessment_config.h"
#include "embedded_speech_config.h"
#include "speech_recognition_model.h"
#include "speech_translation_model.h"
#include "source_language_recognizer.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


SPX_EXTERN_C void* SRLib_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    using namespace USP;
    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY(CSpxAudioStreamSession, ISpxSession);
        SPX_FACTORY_MAP_ENTRY(CSpxRecognitionEventArgs, ISpxRecognitionEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxActivityEventArgs, ISpxActivityEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxTurnStatusEventArgs, ISpxTurnStatusEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxRecognitionResult, ISpxRecognitionResult);
        SPX_FACTORY_MAP_ENTRY(CSpxRecognizer, ISpxRecognizer);
        SPX_FACTORY_MAP_ENTRY(CSpxSourceLanguageRecognizer, ISpxRecognizer);
        SPX_FACTORY_MAP_ENTRY(CSpxDialogServiceConnector, ISpxRecognizer);
        SPX_FACTORY_MAP_ENTRY(CSpxConversationTranscriberV2, ISpxRecognizer);
        SPX_FACTORY_MAP_ENTRY(CSpxMeetingTranscriber, ISpxRecognizer);
        SPX_FACTORY_MAP_ENTRY(CSpxMeeting, ISpxMeeting);
        SPX_FACTORY_MAP_ENTRY(CSpxParticipant, ISpxParticipant);
        SPX_FACTORY_MAP_ENTRY(CSpxUser, ISpxUser);
        SPX_FACTORY_MAP_ENTRY(CSpxKwsModel, ISpxKwsModel);
        SPX_FACTORY_MAP_ENTRY(CSpxPhrase, ISpxPhrase);
        SPX_FACTORY_MAP_ENTRY(CSpxPhraseListGrammar, ISpxPhraseList);
        SPX_FACTORY_MAP_ENTRY(CSpxTranslationRecognizer, ISpxRecognizer);
        SPX_FACTORY_MAP_ENTRY(CSpxSessionEventArgs, ISpxSessionEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxTokenReqeustEventArgs, ISpxSessionEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxUspRecoEngineAdapter, ISpxRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxUspRecoEngineAdapterRetry, ISpxRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxUspRecoEngineAdapterRetry_OffsetFixupWrapper, ISpxRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxHybridRecoEngineAdapter, ISpxRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxUspCallbackWrapper, ISpxUspCallbacks);
        SPX_FACTORY_MAP_ENTRY(CSpxSpeechAudioProcessor, ISpxSpeechAudioProcessorAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxConnection, ISpxConnection);
        SPX_FACTORY_MAP_ENTRY(CSpxConnectionEventArgs, ISpxConnectionEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxConnectionMessage, ISpxConnectionMessage);
        SPX_FACTORY_MAP_ENTRY(CSpxConnectionMessageEventArgs, ISpxConnectionMessageEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxStoredGrammar, ISpxStoredGrammar);
        SPX_FACTORY_MAP_ENTRY(CSpxClassLanguageModel, ISpxClassLanguageModel);
        SPX_FACTORY_MAP_ENTRY(CSpxSpeechConfig, ISpxSpeechConfig);
        SPX_FACTORY_MAP_ENTRY(CSpxSpeechTranslationConfig, ISpxSpeechConfig);
        SPX_FACTORY_MAP_ENTRY(CSpxAutoDetectSourceLangConfig, ISpxAutoDetectSourceLangConfig);
        SPX_FACTORY_MAP_ENTRY(CSpxSourceLanguageConfig, ISpxSourceLanguageConfig);
        SPX_FACTORY_MAP_ENTRY(CSpxOutputRecoEngineAdapter, ISpxRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxAudioSessionShim, ISpxAudioSessionShim);
        SPX_FACTORY_MAP_ENTRY(CSpxPronunciationAssessmentConfig, ISpxPronunciationAssessmentConfig);
        SPX_FACTORY_MAP_ENTRY(CSpxEmbeddedSpeechConfig, ISpxSpeechConfig);
        SPX_FACTORY_MAP_ENTRY(CSpxSpeechRecognitionModel, ISpxSpeechRecognitionModel);
        SPX_FACTORY_MAP_ENTRY(CSpxSpeechTranslationModel, ISpxSpeechTranslationModel);
    SPX_FACTORY_MAP_END();
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
