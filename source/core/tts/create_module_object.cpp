//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_module_object.cpp: Implementation definitions for *CreateModuleObject* methods
//

#include "stdafx.h"

#include "factory_helpers.h"
#include "synthesizer.h"
#include "synthesis_request.h"
#include "synthesis_result.h"
#include "synthesis_event_args.h"
#include "word_boundary_event_args.h"
#include "viseme_event_args.h"
#include "bookmark_event_args.h"
#include "voices_list_result.h"
#include "voice_info.h"
#include "thread_service.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


SPX_EXTERN_C void* TTSLib_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY(CSpxSynthesizer, ISpxSynthesizer);
        SPX_FACTORY_MAP_ENTRY(CSpxSynthesisResult, ISpxSynthesisResult);
        SPX_FACTORY_MAP_ENTRY(CSpxSynthesisEventArgs, ISpxSynthesisEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxWordBoundaryEventArgs, ISpxWordBoundaryEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxVisemeEventArgs, ISpxVisemeEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxBookmarkEventArgs, ISpxBookmarkEventArgs);
        SPX_FACTORY_MAP_ENTRY(CSpxThreadService, ISpxThreadService);
        SPX_FACTORY_MAP_ENTRY(CSpxSynthesisVoicesResult, ISpxSynthesisVoicesResult);
        SPX_FACTORY_MAP_ENTRY(CSpxVoiceInfo, ISpxVoiceInfo);
        SPX_FACTORY_MAP_ENTRY(CSpxSynthesisRequest, ISpxSynthesisRequest);
    SPX_FACTORY_MAP_END();
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
