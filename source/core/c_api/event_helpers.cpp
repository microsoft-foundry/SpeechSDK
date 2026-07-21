//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// event_helpers.cpp: Private implementation definitions for EventSignal related C methods
//
#include "stdafx.h"
#include "event_helpers.h"
#include "handle_table.h"
#include <interface_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPXAPI_PRIVATE recognizer_session_set_event_callback(ISpxRecognizerEvents::SessionEvent_Type ISpxRecognizerEvents::*psessionEvent, SPXRECOHANDLE hreco, PSESSION_CALLBACK_FUNC pCallback, void* pvContext)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        auto recognizer = (*recohandles)[hreco];

        auto pfn = [=](std::shared_ptr<ISpxSessionEventArgs> e) {
            auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSessionEventArgs, SPXEVENTHANDLE>();
            auto hevent = eventhandles->TrackHandle(e);
            (*pCallback)(hreco, hevent, pvContext);
        };

        auto pISpxRecognizerEvents = SpxQueryInterface<ISpxRecognizerEvents>(recognizer).get();
        (pISpxRecognizerEvents->*psessionEvent).UnregisterAllCallbacks();

        if (pCallback != nullptr)
        {
            (pISpxRecognizerEvents->*psessionEvent).RegisterCallback(pfn);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_PRIVATE recognizer_recognition_set_event_callback(ISpxRecognizerEvents::RecoEvent_Type ISpxRecognizerEvents::*precoEvent, SPXRECOHANDLE hreco, PRECOGNITION_CALLBACK_FUNC pCallback, void* pvContext)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        auto recognizer = (*recohandles)[hreco];

        auto pfn = [=](std::shared_ptr<ISpxRecognitionEventArgs> e) {
            auto eventhandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognitionEventArgs, SPXEVENTHANDLE>();
            auto hevent = eventhandles->TrackHandle(e);
            (*pCallback)(hreco, hevent, pvContext);
        };

        auto pISpxRecognizerEvents = SpxQueryInterface<ISpxRecognizerEvents>(recognizer).get();
        (pISpxRecognizerEvents->*precoEvent).UnregisterAllCallbacks();

        if (pCallback != nullptr)
        {
            (pISpxRecognizerEvents->*precoEvent).RegisterCallback(pfn);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_PRIVATE connection_set_event_callback(ISpxRecognizerEvents::ConnectionEvent_Type ISpxRecognizerEvents::*connectionEvent, SPXCONNECTIONHANDLE connectionHandle, CONNECTION_CALLBACK_FUNC callback, void* context)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, connectionEvent == nullptr);
        auto connectionHandleTable = CSpxSharedPtrHandleTableManager::Get<ISpxConnection, SPXCONNECTIONHANDLE>();
        auto connection = (*connectionHandleTable)[connectionHandle];

        auto pfn = [=](std::shared_ptr<ISpxConnectionEventArgs> e) {
            auto connectionEventHandleTable = CSpxSharedPtrHandleTableManager::Get<ISpxConnectionEventArgs, SPXEVENTHANDLE>();
            auto eventHandle = connectionEventHandleTable->TrackHandle(e);
            (*callback)(eventHandle, context);
        };

        auto recognizer = connection->GetRecognizer();
        // for Disconnect() call, if the recognizer is not valid, we just return.
        SPX_THROW_HR_IF(SPXERR_INVALID_RECOGNIZER, recognizer == nullptr && callback != nullptr);
        if (recognizer != nullptr)
        {
            auto pISpxRecognizerEvents = SpxQueryInterface<ISpxRecognizerEvents>(recognizer).get();
            SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, pISpxRecognizerEvents == nullptr);
            (pISpxRecognizerEvents->*connectionEvent).UnregisterAllCallbacks();

            if (callback != nullptr)
            {
                (pISpxRecognizerEvents->*connectionEvent).RegisterCallback(pfn);
            }
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_PRIVATE connection_message_set_event_callback(ISpxRecognizerEvents::ConnectionMessageEvent_Type ISpxRecognizerEvents::*connectionMessageEvent, SPXCONNECTIONHANDLE connectionHandle, CONNECTION_CALLBACK_FUNC callback, void* context)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, connectionMessageEvent == nullptr);
        auto connection = CSpxSharedPtrHandleTableManager::GetPtr<ISpxConnection, SPXCONNECTIONHANDLE>(connectionHandle);

        auto pfn = [=](std::shared_ptr<ISpxConnectionMessageEventArgs> e) {
            auto eventHandle = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxConnectionMessageEventArgs, SPXEVENTHANDLE>(e);
            (*callback)(eventHandle, context);
        };

        auto recognizer = connection->GetRecognizer();
        SPX_THROW_HR_IF(SPXERR_INVALID_RECOGNIZER, recognizer == nullptr && callback != nullptr);
        if (recognizer == nullptr)
        {
            return hr;
        }

        auto pISpxRecognizerEvents = SpxQueryInterface<ISpxRecognizerEvents>(recognizer).get();
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, pISpxRecognizerEvents == nullptr);
        (pISpxRecognizerEvents->*connectionMessageEvent).UnregisterAllCallbacks();

        if (callback != nullptr)
        {
            (pISpxRecognizerEvents->*connectionMessageEvent).RegisterCallback(pfn);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

template<typename EventInterface, typename EventArgs, typename Event>
SPXHR dialog_service_connector_set_event_callback(Event event, SPXRECOHANDLE h_connector, PRECOGNITION_CALLBACK_FUNC p_callback, void* pv_context)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxDialogServiceConnector, SPXRECOHANDLE>();
        auto connector = (*handles)[h_connector];

        auto pfn = [=](std::shared_ptr<EventArgs> e) {
            auto eventhandles = CSpxSharedPtrHandleTableManager::Get<EventArgs, SPXEVENTHANDLE>();
            auto h_event = eventhandles->TrackHandle(e);
            (*p_callback)(h_connector, h_event, pv_context);
        };

        auto events = SpxQueryInterface<EventInterface>(connector).get();
        (events->*event).UnregisterAllCallbacks();

        if (p_callback != nullptr)
        {
            (events->*event).RegisterCallback(pfn);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_PRIVATE dialog_service_connector_session_set_event_callback(ISpxRecognizerEvents::SessionEvent_Type ISpxRecognizerEvents::*p_session_event, SPXRECOHANDLE h_connector, PSESSION_CALLBACK_FUNC p_callback, void* pv_context)
{
    return dialog_service_connector_set_event_callback<ISpxRecognizerEvents, ISpxSessionEventArgs>(p_session_event, h_connector, p_callback, pv_context);
}

SPXAPI_PRIVATE dialog_service_connector_recognition_set_event_callback(ISpxRecognizerEvents::RecoEvent_Type ISpxRecognizerEvents::*p_reco_event, SPXRECOHANDLE h_connector, PRECOGNITION_CALLBACK_FUNC p_callback, void* pv_context)
{
    return dialog_service_connector_set_event_callback<ISpxRecognizerEvents, ISpxRecognitionEventArgs>(p_reco_event, h_connector, p_callback, pv_context);
}

SPXAPI_PRIVATE dialog_service_connector_activity_received_set_event_callback(ISpxDialogServiceConnectorEvents::ActivityReceivedEvent_Type ISpxDialogServiceConnectorEvents::*p_act_event, SPXRECOHANDLE h_connector, PRECOGNITION_CALLBACK_FUNC p_callback, void* pv_context)
{
    return dialog_service_connector_set_event_callback<ISpxDialogServiceConnectorEvents, ISpxActivityEventArgs>(p_act_event, h_connector, p_callback, pv_context);
}

SPXAPI_PRIVATE dialog_service_connector_turn_status_received_set_event_callback(ISpxDialogServiceConnectorEvents::TurnStatusEvent_Type ISpxDialogServiceConnectorEvents::* p_act_event, SPXRECOHANDLE h_connector, PRECOGNITION_CALLBACK_FUNC p_callback, void* pv_context)
{
    return dialog_service_connector_set_event_callback<ISpxDialogServiceConnectorEvents, ISpxTurnStatusEventArgs>(p_act_event, h_connector, p_callback, pv_context);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
