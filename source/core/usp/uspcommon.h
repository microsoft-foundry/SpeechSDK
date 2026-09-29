//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// uspcommon.h: common definitions and declaration used by USP internal implementation
//

#pragma once

#include <interfaces/proxy_server_info.h>
#include <http_headers.h>
#include "usperror.h"

#define UNUSED(x) (void)(x)

#define USE_BUFFER_SIZE    ((size_t)-1)

#if defined _MSC_VER
#define PROTOCOL_VIOLATION(__fmt, ...)  SPX_TRACE_ERROR("ProtocolViolation:" __fmt, __VA_ARGS__)
#else
#define PROTOCOL_VIOLATION(__fmt, ...)  SPX_TRACE_ERROR("ProtocolViolation:" __fmt, ##__VA_ARGS__)
#endif

#include <string>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

    class path
    {
    public:
        static const char* speechHypothesis;
        static const char* speechTentativePhrase;
        static const char* speechPhrase;
        static const char* speechFragment;
        static const char* speechKeyword;
        static const char* turnStart;
        static const char* turnEnd;
        static const char* speechStartDetected;
        static const char* speechEndDetected;
        static const char* translationHypothesis;
        static const char* translationPhrase;
        static const char* translationSynthesis;
        static const char* translationSynthesisEnd;
        static const char* translationResponse;
        static const char* audio;
        static const char* audioMetaData;
        static const char* audioStart;
        static const char* audioEnd;
        // Inline commit: outbound commit request.
        static const char* audioCommit;
    };

    class json_properties
    {
    public:
        static const char* offset;
        static const char* duration;
        static const char* status;
        static const char* text;
        static const char* recoStatus;
        static const char* displayText;
        static const char* context;
        static const char* tag;
        static const char* speaker;
        static const char* utteranceId;
        static const char* phraseId;
        static const char* nbest;
        static const char* confidence;
        static const char* display;

        // Inline commit: JSON field in speech.phrase body carrying echoed
        // X-Client-* headers from the originating audio.commit.
        static const char* clientAudioMetadata;
        // Key inside clientAudioMetadata identifying the echoed commit token.
        static const char* commitTokenHeader;

        static const char* translation;
        static const char* translationStatus;
        static const char* failureReason;
        static const char* translations;
        static const char* synthesisStatus;
        static const char* lang;
        static const char* translationLanguage;

        static const char* metadata;
        static const char* type;
        static const char* data;
        static const char* textBoundary;
        static const char* wordBoundary;
        static const char* sentenceBoundary;
        static const char* boundaryType;
        static const char* viseme;
        static const char* bookmark;
        static const char* sessionEnd;
        static const char* lowerText;
        static const char* visemeId;
        static const char* animationChunk;
        static const char* isLastAnimation;

        static const char* primaryLanguage;
        static const char* speechHypothesis;
        static const char* speechTentativePhrase;
        static const char* speechPhrase;
    };

}
}
}
}

