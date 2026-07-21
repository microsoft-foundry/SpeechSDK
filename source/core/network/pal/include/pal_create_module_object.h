//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <azac_api_c_common.h>

#if defined(PAL_CONFIG_EXPORTAPIS)
    #define PAL_API_EXPORT         AZAC_DLL_EXPORT
#else
    #define PAL_API_EXPORT         AZAC_DLL_IMPORT
#endif

#define PAL_API AZAC_EXTERN_C PAL_API_EXPORT AZAC_API_RESULTTYPE AZAC_API_NOTHROW AZAC_API_CALLTYPE

/// <summary>
/// Gets a named value/function from the PAL library
/// </summary>
/// <param name="pszName">The name of the value/function to retrieve</param>
/// <param name="pValue">Pointer to the value to set</param>
/// <returns>AZAC_ERR_NONE on success, AZAC_ERR_NOT_FOUND if the name specified is unknown,
/// AZAC_ERR_INVALID_ARG if either argument passed is null, or AZAC_ERR_RUNTIME_ERROR
/// in the case of errors</returns>
PAL_API pal_get_platform(void** ppValue);

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    // We're going to have 2 distinct CMO's for the network PAL for now.
    // At some future point it will likely make sense / be the right thing to detach all but a dynamic PAL into a standalone module
    // and be able to package / ship independently. But for now we'll keep both linked into the core module.

    // A PAL that is statically linked in based on the cmake setting to use Azure-Core, Azure-C-Shared, or nothing.
    AZAC_EXTERN_C void* Pal_CreateModuleObject(const char* className, uint64_t interfaceTypeId);

    // A CMO that directs requests for network objects to the language binding layer.
    AZAC_EXTERN_C void* Pal_CreateModuleObject_Binding(const char* className, uint64_t interfaceTypeId);
}}}}
