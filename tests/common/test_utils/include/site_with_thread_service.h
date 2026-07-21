//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "interface_helpers.h"
#include "ispxinterfaces.h"
#include "thread_service.h"
#include "service_helpers.h"
#include "ispx_telemetry_store_impl.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

class CSpxSiteWithThreadService :
    public CSpxThreadService,
    public ISpxGenericSite,
    public ISpxServiceProvider,
    public ISpxTelemetryStoreImpl    
{
    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxTelemetryStore)
        SPX_INTERFACE_MAP_FUNC(CSpxThreadService::QueryInterface)
    SPX_INTERFACE_MAP_END()

    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxThreadService)
        SPX_SERVICE_MAP_ENTRY(ISpxTelemetryStore)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()
};
