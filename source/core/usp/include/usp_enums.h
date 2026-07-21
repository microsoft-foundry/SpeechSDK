//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

enum class EndpointType { Speech, Translation, TranslationV1, Dialog, ConversationTranscriptionService, ConversationTranscriptionServiceV2, DynamicConversationTranscriptionService, SpeechSynthesis, CustomVoice, StandaloneLanguageId };

enum class RecognitionMode : unsigned int { Interactive = 0, Conversation = 1, Dictation = 2 };

enum class LanguageIdMode : unsigned int { DetectAtAudioStart = 0, DetectContinuous = 1, DetectSegments = 2 };

enum class LanguageIdPriority : unsigned int { PrioritizeLatency = 0, PrioritizeAccuracy = 1 }; // Note that since 1.25.0, Carbon only uses "Latency".

enum class OutputFormat : unsigned int { Simple = 0, Detailed = 1 };

enum class AuthenticationType : size_t { SubscriptionKey = 0, AuthorizationToken, SearchDelegationRPSToken, SIZE_AUTHENTICATION_TYPE };

}}}}
