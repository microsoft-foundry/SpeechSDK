//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_module_object.cpp: Implementation definitions for *CreateModuleObject* methods
//
#include "stdafx.h"
#include "speech_api_factory.h"
#include "speech_synthesis_api_factory.h"
#include "factory_helpers.h"
#include "module_factory.h"
#include "named_properties.h"
#include "pal_create_module_object.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

#ifdef STATIC_CODEC_EXTENSION
SPX_EXTERN_C void* Codec_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
#ifdef STATIC_CUSTOM_COMMANDS_EXTENSION
SPX_EXTERN_C void* CustomCommandsExtension_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
#ifdef STATIC_KWS_EXTENSION
SPX_EXTERN_C void* SDKKWS_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
#ifdef STATIC_AUDIO_EXTENSION
SPX_EXTERN_C void* AudioExtension_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
#ifdef STATIC_EMBEDDEDSR_EXTENSION
#ifdef BUILD_RNNT
SPX_EXTERN_C void* RNNT_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
#endif
#ifdef STATIC_VAD_EXTENSION
SPX_EXTERN_C void* VAD_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
#ifdef STATIC_HYBRIDTTS_EXTENSION
SPX_EXTERN_C void* HYBRID_TTS_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
#ifdef STATIC_MAS_EXTENSION
SPX_EXTERN_C void* MAS_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
#endif
SPX_EXTERN_C void* AudioLib_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
SPX_EXTERN_C void* DataLib_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
SPX_EXTERN_C void* SRLib_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
SPX_EXTERN_C void* TTSLib_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
SPX_EXTERN_C void* TTS_Cloud_CreateModuleObject(const char* className, uint64_t interfaceTypeId);
SPX_EXTERN_C void* USP_CreateModuleObject(const char* className, uint64_t interfaceTypeId);

SPX_EXTERN_C void* IntraAssemblyCreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    SPX_FACTORY_MAP_BEGIN();
        #ifdef STATIC_CODEC_EXTENSION
            SPX_FACTORY_MAP_ENTRY_FUNC(Codec_CreateModuleObject);
        #endif
        #ifdef STATIC_KWS_EXTENSION
            SPX_FACTORY_MAP_ENTRY_FUNC(SDKKWS_CreateModuleObject);
        #endif
        #ifdef STATIC_AUDIO_EXTENSION
            SPX_FACTORY_MAP_ENTRY_FUNC(AudioExtension_CreateModuleObject);
        #endif
        #ifdef STATIC_CUSTOM_COMMANDS_EXTENSION
            SPX_FACTORY_MAP_ENTRY_FUNC(CustomCommandsExtension_CreateModuleObject);
        #endif
        #ifdef STATIC_EMBEDDEDSR_EXTENSION
        #ifdef BUILD_RNNT
            SPX_FACTORY_MAP_ENTRY_FUNC(RNNT_CreateModuleObject);
        #endif
        #endif
        #ifdef STATIC_VAD_EXTENSION
            SPX_FACTORY_MAP_ENTRY_FUNC(VAD_CreateModuleObject);
        #endif
        #ifdef STATIC_HYBRIDTTS_EXTENSION
            SPX_FACTORY_MAP_ENTRY_FUNC(HYBRID_TTS_CreateModuleObject);
        #endif
        #ifdef STATIC_MAS_EXTENSION
            SPX_FACTORY_MAP_ENTRY_FUNC(MAS_CreateModuleObject);
        #endif
        SPX_FACTORY_MAP_ENTRY_FUNC(AudioLib_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY_FUNC(DataLib_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY_FUNC(SRLib_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY_FUNC(TTSLib_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY_FUNC(TTS_Cloud_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY_FUNC(Pal_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY_FUNC(USP_CreateModuleObject);
        SPX_FACTORY_MAP_ENTRY(CSpxSpeechApiFactory, ISpxSpeechApiFactory);
        SPX_FACTORY_MAP_ENTRY(CSpxSpeechSynthesisApiFactory, ISpxSpeechSynthesisApiFactory);
        SPX_FACTORY_MAP_ENTRY(CSpxNamedProperties, ISpxNamedProperties);
    SPX_FACTORY_MAP_END();
}

void UpdateFactories(std::list<std::shared_ptr<ISpxObjectFactory>>& moduleFactories, std::shared_ptr<ISpxObjectFactory> factory)
{
    // If the factory is null, do not add it.
    if (factory == nullptr) return;

    // If the factory is already in the list, do not add it again.
    for (auto& f : moduleFactories)
    {
        if (f == factory)
        {
            return;
        }
    }
    moduleFactories.push_back(factory);
}

SPX_EXTERN_C void AddExtensionModules(std::list<std::shared_ptr<ISpxObjectFactory>>& moduleFactories)
{
    SPX_DBG_TRACE_FUNCTION();
    std::shared_ptr<ISpxObjectFactory> factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.mas" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.kws" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.vad" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
#ifndef __MACH__
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.codec" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
#endif
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.audio.sys" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.customcommands" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.embedded.sr" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.embedded.tts" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
    factory = CSpxModuleFactory::Get(_LIB_PREFIX_ "Microsoft.CognitiveServices.Speech.extension.telemetry" _LIB_EXT_);
    UpdateFactories(moduleFactories, factory);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
