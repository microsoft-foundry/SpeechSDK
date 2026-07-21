//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "binding_helpers.h"
#include "binding_callback_storage.h"
#include <interface_helpers.h>
#include "result_helpers.h"
#include "../network/pal/pal_binding/http_response.h"
#include "handle_helpers.h"
#include <functional>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Define StreamedResponseDataHandler to match the one in http_response.h
using StreamedResponseDataHandler = std::function<void(const uint8_t*, size_t)>;

SPXAPI register_send_callback(PBINDING_CALLBACK_FUNC pCallback, void* pvContext)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pCallback == nullptr);
        CallbackStorage::Instance().Store(pCallback, pvContext);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI process_streaming_data(SPXEVENTHANDLE hresponse, const uint8_t* data, size_t size)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        // Validate inputs
        SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, data == nullptr);
        SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, size == 0);
        
        // If hresponse is null, this is a non-streaming request, so just return success
        if (hresponse == nullptr)
        {
            SPX_TRACE_INFO("Non-streaming request - data processed but no response callback needed");
            return 0; // SUCCESS for non-streaming requests
        }
        
        // Get the response object from the handle table
        auto httpResponsePtr = SpxTryGetPtrFromHandle<ISpxHttpResponse>(hresponse);
        if (httpResponsePtr == nullptr)
        {
            SPX_TRACE_ERROR("Failed to get response object from handle table");
            return SPXERR_INVALID_ARG;
        }
        
        try
        {
            // Use static_cast which is safer than reinterpret_cast
            // This is safe because we know the concrete type we registered
            auto concreteResponse = static_cast<CSpxBindingBasedHttpResponse*>(httpResponsePtr.get());
            concreteResponse->SignalOnDataCallback(data, size);
            return 0; // SUCCESS
        }
        catch (const std::exception& ex)
        {
            SPX_TRACE_ERROR("Exception in SignalOnDataCallback: %s", ex.what());
            return SPXERR_RUNTIME_ERROR;
        }
        catch (...)
        {
            SPX_TRACE_ERROR("Unknown exception in SignalOnDataCallback");
            return SPXERR_RUNTIME_ERROR;
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI http_eventargs_get_property_bag(SPXEVENTHANDLE hevent, SPXPROPERTYBAGHANDLE* hpropbag)
{
    // Get the property bag handle directly
    return CSpxApiManager::QueryInterface<SPXEVENTHANDLE, CSpxHttpEventArgs, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hevent, hpropbag);
}


}
}
}
} // Microsoft::CognitiveServices::Speech::Impl
