//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "boundary_type_enum_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<>
bool EnumHelpers::TryParse<SpeechSynthesisBoundaryType>(const char* string, SpeechSynthesisBoundaryType& value)
{
    ENUM_PARSE(SpeechSynthesisBoundaryType::Word, "WordBoundary");
    ENUM_PARSE(SpeechSynthesisBoundaryType::Punctuation, "PunctuationBoundary");
    ENUM_PARSE(SpeechSynthesisBoundaryType::Sentence, "SentenceBoundary");

    return false;
}
}}}}
