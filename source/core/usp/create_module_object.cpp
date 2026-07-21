//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include "factory_helpers.h"
#include "include/usp_connection.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace Microsoft::CognitiveServices::Speech::USP;

SPX_EXTERN_C void* USP_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY(CSpxUspConnection, ISpxUspConnection);
        SPX_FACTORY_MAP_ENTRY(CSpxTelemetry, ISpxTelemetryBase);
    SPX_FACTORY_MAP_END();
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
