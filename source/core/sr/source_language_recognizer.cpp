//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#include <sstream>
#include "stdafx.h"
#include "source_language_recognizer.h"
#include "service_helpers.h"
#include "site_helpers.h"
#include "string_utils.h"
#include "speech_config.h"
#include <ajv.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxSourceLanguageRecognizer::CSpxSourceLanguageRecognizer()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxSourceLanguageRecognizer::~CSpxSourceLanguageRecognizer()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxSourceLanguageRecognizer::Init()
{
    CSpxRecognizer::Init();
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
