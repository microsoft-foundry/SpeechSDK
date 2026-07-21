//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "handle_helpers.h"
#include "service_helpers.h"
#include "site_helpers.h"
#include "mock_controller.h"
#include "property_id_2_name_map.h"
#include "ispxinterfaces.h"
#include "handle_helpers.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI property_bag_create(SPXPROPERTYBAGHANDLE* hpropbag)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hpropbag == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hpropbag = SPXHANDLE_INVALID;

        auto prop = SpxCreateObjectWithSite<ISpxNamedProperties>("CSpxNamedProperties", SpxGetRootSite());

        auto prophandles = CSpxSharedPtrHandleTableManager::Get<ISpxNamedProperties, SPXPROPERTYBAGHANDLE>();

        *hpropbag = prophandles->TrackHandle(prop);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

std::shared_ptr<ISpxNamedProperties> property_bag_from_handle(SPXPROPERTYBAGHANDLE hpropbag)
{
    std::shared_ptr<ISpxNamedProperties> namedProperties;
    if (SPXFACTORYHANDLE_ROOTSITEPARAMETERS_MOCK == hpropbag)
    {
        namedProperties = SpxQueryService<ISpxNamedProperties>(SpxGetRootSite());
    }
    else
    {
        namedProperties = SpxGetPtrFromHandle<ISpxNamedProperties>(hpropbag);
    }
    return namedProperties;
}


SPXAPI_(bool) property_bag_is_valid(SPXPROPERTYBAGHANDLE hpropbag)
{
    return CSpxApiManager::IsValid<SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hpropbag);
}

SPXAPI property_bag_release(SPXPROPERTYBAGHANDLE hpropbag)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hpropbag);
}
/*
  if name != nullptr, use name + ignore id; if name== nullptr, use id.
  NOTE: Free allocated memory from the returned address using property_bag_free_string() method after usage
*/
SPXAPI__(const char*) property_bag_get_string(SPXPROPERTYBAGHANDLE hpropbag, int id, const char* name, const char* defaultValue)
{
    char* result = nullptr;

    if (hpropbag == nullptr)
    {
        return result;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto namedProperties = property_bag_from_handle(hpropbag);

        const char* name_in_use = name ? name : GetPropertyName(static_cast<PropertyId>(id));
        if (name_in_use == nullptr)
        {
            SPX_TRACE_ERROR("undefined PropertyId of %d", static_cast<int>(id));
            SPX_THROW_ON_FAIL(SPXERR_INVALID_ARG);
        }
        auto tempValue = namedProperties->GetStringValue(name_in_use, defaultValue);
        auto size = tempValue.size() + 1;

        result = new char[size];
        PAL::strcpy(result, size, tempValue.c_str(), size, true);
    }
    SPXAPI_CATCH_AND_RETURN(hr, result);
}

SPXAPI property_bag_free_string(const char* value)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        if (value)
            delete[] value;
    }
    /*
    * This code is often invoked as clean up code, or on function exit. Using the regular SPXAPI_CATCH_AND_RETURN_HR
    * macro has the side effect of storing any exceptions in the handle table. This means we now have to check the HR
    * and remember to remove the stored exception from the handle table. There are several problems here:
    * - Most places in the code don't bother to check the return value. This results in memory leaks as we lose the
    *   exception handle
    * - The default handlers for dealing with non-success codes throw exceptions. Since this can be called as part
    *   of the cleanup code when unwinding the stack for other exceptions, it is bad form to throw an exception
    * - Adding new handlers to retrieve, and log exceptions (but not throw) for all callers of this method seems
    *   excessive
    * Instead, replace with another macro that logs all errors and exceptions, and always returns success. To minimize
    * code changes elsewhere, return the success error code.
    *
    * -> In the future we may want to instead have a void return type (or possible bool) instead
    */
    SPXAPI_CATCH_AND_RETURN(hr, SPX_NOERROR);
}

SPXAPI property_bag_set_string(SPXPROPERTYBAGHANDLE hpropbag, int id, const char* name, const char* defaultValue)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hpropbag == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, defaultValue == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        // Use the char* property name if provided, falling back to the ID if not provided, and error out if the ID
        // wasn't valid.
        if (!name)
        {
            name = GetPropertyName(static_cast<PropertyId>(id));
            if (!name)
            {
                SPX_TRACE_ERROR("undefined PropertyId of %d", static_cast<int>(id));
                SPX_THROW_HR(SPXERR_INVALID_ARG);
            }
        }
        auto namedProperties = property_bag_from_handle(hpropbag);
        namedProperties->SetStringValue(name, defaultValue);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI property_bag_copy(SPXPROPERTYBAGHANDLE hfrom, SPXPROPERTYBAGHANDLE hto)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_HANDLE, hfrom == SPXHANDLE_INVALID);
    SPX_RETURN_HR_IF(SPXERR_INVALID_HANDLE, hto == SPXHANDLE_INVALID);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto fromProperties = property_bag_from_handle(hfrom);
        auto toProperties = property_bag_from_handle(hto);

        toProperties->Copy(fromProperties, false);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

