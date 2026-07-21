//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_module_object.cpp: Implementation definitions for *CreateModuleObject* methods
//

#include "stdafx.h"
#include "create_module_object.h"
#include "create_object_helpers.h"
#include "factory_helpers.h"
#include "mock_audio_reader.h"
#include "mock_interactive_microphone.h"
#include "mock_kws_engine_adapter.h"
#include "mock_reco_engine_adapter.h"
#include "mock_wav_file_reader.h"
#include "mock_wav_file_pump.h"
#include "mock_controller.h"
#include "try_catch_helpers.h"
#include "mocks_manager.h"
#include "mock_vad_engine_adapter.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_EXTERN_C void* CreateMockModuleObject(const char* className, uint64_t interfaceTypeId)
{
    return CSpxMocks::Instance().GetMock(interfaceTypeId, className);
}

bool ShouldMock(const char * mockParameterName)
{
    return SpxGetMockParameterBool(mockParameterName, false);
}

SPX_EXTERN_C void* Mock_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    SPX_DBG_TRACE_VERBOSE("%s trying to create %s, iid %" PRIu64 ".", __FUNCTION__, className, interfaceTypeId);

    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY(CSpxMockAudioReader, ISpxAudioStreamReader);
        SPX_FACTORY_MAP_ENTRY(CSpxMockInteractiveMicrophone, ISpxAudioPump);
        SPX_FACTORY_MAP_ENTRY(CSpxMockKwsEngineAdapter, ISpxDetectorEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxMockVadEngineAdapter, ISpxDetectorEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxMockRecoEngineAdapter, ISpxRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY(CSpxMockWavFileReader, ISpxAudioFile);
        SPX_FACTORY_MAP_ENTRY(CSpxMockWavFilePump, ISpxAudioFile);
        SPX_FACTORY_MAP_ENTRY_IF(ShouldMock("CARBON-INTERNAL-MOCK-SdkKwsEngine"), CSpxSdkKwsEngineAdapter, ISpxDetectorEngineAdapter, CSpxMockKwsEngineAdapter);
        SPX_FACTORY_MAP_ENTRY_IF(ShouldMock("CARBON-INTERNAL-MOCK-SdkVadEngine"), CSpxSdkVadEngineAdapter, ISpxDetectorEngineAdapter, CSpxMockVadEngineAdapter);
        SPX_FACTORY_MAP_ENTRY_IF(ShouldMock("CARBON-INTERNAL-MOCK-UspRecoEngine"), CSpxUspRecoEngineAdapter, ISpxRecoEngineAdapter, CSpxMockRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY_IF(ShouldMock("CARBON-INTERNAL-MOCK-RnntRecoEngine"), CSpxRnntRecoEngineAdapter, ISpxRecoEngineAdapter, CSpxMockRecoEngineAdapter);
        SPX_FACTORY_MAP_ENTRY_IF(ShouldMock("CARBON-INTERNAL-MOCK-Microphone"), CSpxInteractiveMicrophone, ISpxAudioPump, CSpxMockInteractiveMicrophone);
        SPX_FACTORY_MAP_ENTRY_IF(ShouldMock("CARBON-INTERNAL-MOCK-WavFileReader"), CSpxWavFileReader, ISpxAudioFile, CSpxMockWavFileReader);
        SPX_FACTORY_MAP_ENTRY_IF(ShouldMock("CARBON-INTERNAL-MOCK-WavFilePump"), CSpxWavFilePump, ISpxAudioFile, CSpxMockWavFilePump);
        SPX_FACTORY_MAP_ENTRY_FUNC(CreateMockModuleObject)
    SPX_FACTORY_MAP_END();
}

SPX_EXTERN_C SPXDLL_EXPORT void* CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY_FUNC(Mock_CreateModuleObject);
    SPX_FACTORY_MAP_END();
}

/// <summary>
/// Used for testing. This allows you to set a callback function to control the instances generated
/// </summary>
/// <param name="interfaceTypeId">The id of the interface we want to mock</param>
/// <param name="className">The name of the instance of that interface we want to mock. Set to nullptr to mock for *all* interface instances</param>
/// <param name="context">Pointer to the context to be passed to the generator function</param>
/// <param name="func">Pointer to the function to generate the instance</param>
SPX_EXTERN_C SPXDLL_EXPORT SPXAPI_RESULTTYPE SPXAPI_NOTHROW SPXAPI_CALLTYPE mock_add_handler(uint64_t interfaceTypeId, const char* className, void* context, void* (*func)(void*))
{
    std::string error;

    SPXAPI_TRY()
    {
        CSpxMocks::Instance().AddMock(interfaceTypeId, className == nullptr ? "" : className, context, func);
        return SPX_NOERROR;
    }
    SPXAPI_CATCH_ONLY();

    return SPXERR_RUNTIME_ERROR;
}

/// <summary>
/// Used for testing. This allows you to remove a callback function to control the instances generated
/// </summary>
/// <param name="interfaceTypeId">The id of the interface we want to mock</param>
/// <param name="className">The name of the instance of that interface we want to mock</param>
SPX_EXTERN_C SPXDLL_EXPORT SPXAPI_RESULTTYPE SPXAPI_NOTHROW SPXAPI_CALLTYPE mock_remove_handler(uint64_t interfaceTypeId, const char* className)
{
    std::string error;

    SPXAPI_TRY()
    {
        bool success = CSpxMocks::Instance().RemoveMock(interfaceTypeId, className == nullptr ? "" : className);
        return success ? SPX_NOERROR : SPXERR_NOT_FOUND;
    }
    SPXAPI_CATCH_ONLY();

    return SPXERR_RUNTIME_ERROR;
}

/// <summary>
/// Resets all the registered mocks
/// </summary>
SPX_EXTERN_C SPXDLL_EXPORT SPXAPI_RESULTTYPE SPXAPI_NOTHROW SPXAPI_CALLTYPE mock_reset_handlers()
{
    std::string error;

    SPXAPI_TRY()
    {
        CSpxMocks::Instance().Clear();
        return SPX_NOERROR;
    }
    SPXAPI_CATCH_ONLY();

    return SPXERR_RUNTIME_ERROR;
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
