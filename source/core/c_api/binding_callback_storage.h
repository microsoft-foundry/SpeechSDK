//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// binding_callback_storage.h: Storage and management of binding callbacks
//

#pragma once

#include "stdafx.h"
#include "speechapi_c_common.h"
#include <speechapi_cxx_eventsignalbase.h>
#include "handle_table.h"
#include "binding_helpers.h"
#include "interfaces/types.h"
#include "../network/pal/pal_binding/binding_http_event_args.h"
#include "../network/pal/pal_binding/http_response.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Forward declarations
typedef std::function<void(const uint8_t*, size_t)> StreamedResponseDataHandler;

/// <summary>
/// Singleton class to store and manage callbacks for binding between C++ and C# code
/// </summary>
class CallbackStorage : public EventSignalBase<std::shared_ptr<CSpxHttpEventArgs>>
{
private:
    CallbackStorage() = default;

public:
    // Map to store streaming callbacks - made public for direct access from http_request.cpp
    std::map<std::string, std::unique_ptr<StreamedResponseDataHandler>> m_callbackMap;
    
    static CallbackStorage& Instance()
    {
        static CallbackStorage instance;
        return instance;
    }

    void InitializeCallbacks()
    {
        this->UnregisterAllCallbacks();
    }

    void Store(PBINDING_CALLBACK_FUNC pCallback, void* ctx)
    {
        // Unregister any existing callbacks
        this->UnregisterAllCallbacks();

        // Register the new callback if provided
        if (pCallback != nullptr)
        {
            // Create a lambda function that will be called when the event signal is triggered
            auto callbackWrapper = [pCallback, ctx](std::shared_ptr<CSpxHttpEventArgs> args) {
                if (pCallback && args)
                {
                    // Get the handle table for EventArgs
                    auto eventHandles = CSpxSharedPtrHandleTableManager::Get<CSpxHttpEventArgs, SPXEVENTHANDLE>();

                    // Track the event args in the handle table to get a handle
                    SPXEVENTHANDLE eventHandle = eventHandles->TrackHandle(args);

                    // Call the stored callback function with the real handle
                    pCallback(eventHandle, NULL, ctx);
                }
                };

            // Register the lambda with the EventSignalBase
            this->RegisterCallback(callbackWrapper);
        }
    }

    // Signal the event and return directly - the C# handler will process synchronously
    void SignalSynchronously(std::shared_ptr<CSpxHttpEventArgs> requestArgs)
    {
        // Signal the event - the C# callback will process it synchronously before returning
        this->Signal(requestArgs);
    }

};

} // namespace Impl
} // namespace Speech
} // namespace CognitiveServices
} // namespace Microsoft
