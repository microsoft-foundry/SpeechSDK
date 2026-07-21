//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "common.h"
#include "async_helpers.h"
#include "function_helpers.h"
#include "handle_helpers.h"
#include "event_helpers.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

static bool is_speech_synthesizer_connection(SPXCONNECTIONHANDLE connectionHandle)
{
    // this should throw an exception if the connection handle is invalid
    auto connection = SpxGetPtrFromHandle<ISpxConnection>(connectionHandle);

    auto synthesizerConnection = connection->QueryInterface<ISpxSynthesizerConnection>();
    return synthesizerConnection != nullptr;
}

template<typename T>
SPXHR connection_from_object(SPXHANDLE handle, SPXCONNECTIONHANDLE* connectionHandle)
{
    auto handleValid = CSpxApiManager::IsValid<SPXHANDLE, T>(handle);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, connectionHandle == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !handleValid);
    SPXAPI_INIT_HR_TRY(hr)
    {
        *connectionHandle = SPXHANDLE_INVALID;

        auto connectionProvider = SpxHandleQueryInterface<T, ISpxConnectionFromRecognizer>(handle);
        SPX_THROW_HR_IF(SPXERR_EXPLICIT_CONNECTION_NOT_SUPPORTED_BY_RECOGNIZER, connectionProvider == nullptr);
        auto connection = connectionProvider->GetConnection();

        auto connectionHandleTable = CSpxSharedPtrHandleTableManager::Get<ISpxConnection, SPXCONNECTIONHANDLE>();
        *connectionHandle = connectionHandleTable->TrackHandle(connection);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_from_recognizer(SPXRECOHANDLE recognizerHandle, SPXCONNECTIONHANDLE* connectionHandle)
{
    return connection_from_object<ISpxRecognizer>(recognizerHandle, connectionHandle);
}

SPXAPI connection_from_dialog_service_connector(SPXRECOHANDLE dialogServiceConnectorHandle, SPXCONNECTIONHANDLE* connectionHandle)
{
    return connection_from_object<ISpxDialogServiceConnector>(dialogServiceConnectorHandle, connectionHandle);
}

SPXAPI connection_from_speech_synthesizer(SPXSYNTHHANDLE synthesizerHandle, SPXCONNECTIONHANDLE* connectionHandle)
{
    return connection_from_object<ISpxSynthesizer>(synthesizerHandle, connectionHandle);
}

SPXAPI_(bool) connection_handle_is_valid(SPXCONNECTIONHANDLE handle)
{
    return CSpxApiManager::IsValid<SPXCONNECTIONHANDLE, ISpxConnection>(handle);
}

SPXAPI connection_handle_release(SPXCONNECTIONHANDLE handle)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXCONNECTIONHANDLE, ISpxConnection>(handle);
}

SPXAPI connection_connected_set_callback(SPXCONNECTIONHANDLE connection, CONNECTION_CALLBACK_FUNC callback, void* context)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        // the connected event from a speech synthesizer is a special case so let's check for that
        // to set the right event handler first
        if (is_speech_synthesizer_connection(connection))
        {
            hr = synthesizer_connection_connected_set_callback(connection, callback, context);
        }
        else
        {
            hr = connection_set_event_callback(&ISpxRecognizerEvents::Connected, connection, callback, context);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_disconnected_set_callback(SPXCONNECTIONHANDLE connection, CONNECTION_CALLBACK_FUNC callback, void* context)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        // the disconnected event from a speech synthesizer is a special case so let's check for that
        // to set the right event handler first
        if (is_speech_synthesizer_connection(connection))
        {
            hr = synthesizer_connection_disconnected_set_callback(connection, callback, context);
        }
        else
        {
            hr = connection_set_event_callback(&ISpxRecognizerEvents::Disconnected, connection, callback, context);
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_message_received_set_callback(SPXCONNECTIONHANDLE connection, CONNECTION_CALLBACK_FUNC callback, void* context)
{
    return connection_message_set_event_callback(&ISpxRecognizerEvents::ConnectionMessageReceived, connection, callback, context);
}

SPXAPI connection_open(SPXCONNECTIONHANDLE handle, bool forContinuousRecognition)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_HANDLE, !connection_handle_is_valid(handle));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto connection = SpxGetPtrFromHandle<ISpxConnection>(handle);
        connection->Open(forContinuousRecognition);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_close(SPXCONNECTIONHANDLE handle)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !connection_handle_is_valid(handle));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto connection = SpxGetPtrFromHandle<ISpxConnection>(handle);
        connection->Close();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_set_message_property(SPXCONNECTIONHANDLE handle, const char* path, const char* name, const char* value)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, handle == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, name == nullptr || !(*name));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, name == nullptr || !(*path));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, value == nullptr || !(*value));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto setter = SpxHandleQueryInterface<ISpxConnection, ISpxMessageParamFromUser>(handle);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, setter == nullptr);

        setter->SetParameter(path, name, value);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_send_message_async(SPXCONNECTIONHANDLE handle, const char* path, const char* payload, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, handle == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, payload == nullptr || !(*payload));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, path == nullptr || !(*path));

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto setter = SpxHandleQueryInterface<ISpxConnection, ISpxMessageParamFromUser>(handle);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, setter == nullptr);

        launch_async_op(*setter, resolveOverload<const char *, std::string&&>(&ISpxMessageParamFromUser::SendNetworkMessage),
            phasync,
            path,
            payload);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_send_message(SPXCONNECTIONHANDLE handle, const char* path, const char* payload)
{
    return async_to_sync(
        handle,
        connection_send_message_async,
        connection_send_message_wait_for,
        path,
        payload
    );
}

SPXAPI connection_send_message_data(SPXCONNECTIONHANDLE handle, const char* path, uint8_t* data, uint32_t size)
{
    return async_to_sync(
        handle,
        connection_send_message_data_async,
        connection_send_message_wait_for,
        path,
        data,
        size
    );
}

SPXAPI connection_send_message_data_async(SPXCONNECTIONHANDLE handle, const char* path, uint8_t* data, uint32_t size, SPXASYNCHANDLE* phasync)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, handle == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, path == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, data == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        SPX_THROW_HR_IF(SPXERR_NOT_IMPL, is_speech_synthesizer_connection(handle));
        auto setter = SpxHandleQueryInterface<ISpxConnection, ISpxMessageParamFromUser>(handle);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, setter == nullptr);

        std::vector<uint8_t> payload(data, data + size);

        launch_async_op(*setter, resolveOverload<const char *, std::vector<uint8_t>&&>(&ISpxMessageParamFromUser::SendNetworkMessage),
                phasync,
                path,
                std::move(payload));
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI connection_send_message_wait_for(SPXASYNCHANDLE hasync, uint32_t milliseconds)
{
    bool sendResult;

    auto waitResult = async_operation_wait_for_untracked(hasync, milliseconds, &sendResult);

    if (SPX_NOERROR == waitResult && !sendResult)
    {
        return SPXERR_NETWORK_SEND_FAILED;
    }

    return waitResult;
}

SPXAPI connection_async_handle_release(SPXASYNCHANDLE hasync)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXASYNCHANDLE, CSpxAsyncOp<bool>>(hasync);
}

SPXAPI_(bool) connection_message_received_event_handle_is_valid(SPXCONNECTIONMESSAGEHANDLE handle)
{
    return CSpxApiManager::IsValid<SPXEVENTHANDLE, ISpxConnectionMessageEventArgs>(handle);
}

SPXAPI connection_message_received_event_handle_release(SPXEVENTHANDLE hevent)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXEVENTHANDLE, ISpxConnectionMessageEventArgs>(hevent);
}

SPXAPI connection_message_received_event_get_message(SPXEVENTHANDLE event, SPXCONNECTIONMESSAGEHANDLE* hcm)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        *hcm = SPXHANDLE_INVALID;

        auto connectionEventArgs = SpxGetPtrFromHandle<ISpxConnectionMessageEventArgs>(event);

        auto message = connectionEventArgs->GetMessage();

        *hcm = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxConnectionMessage, SPXCONNECTIONMESSAGEHANDLE>(message);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_(bool) connection_message_handle_is_valid(SPXCONNECTIONMESSAGEHANDLE handle)
{
    return CSpxApiManager::IsValid<SPXCONNECTIONMESSAGEHANDLE, ISpxConnectionMessage>(handle);
}

SPXAPI connection_message_handle_release(SPXCONNECTIONMESSAGEHANDLE handle)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXCONNECTIONMESSAGEHANDLE, ISpxConnectionMessage>(handle);
}

SPXAPI connection_message_get_property_bag(SPXCONNECTIONMESSAGEHANDLE hcm, SPXPROPERTYBAGHANDLE* hpropbag)
{
    return CSpxApiManager::QueryInterface<SPXCONNECTIONMESSAGEHANDLE, ISpxConnectionMessage, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hcm, hpropbag);
}

SPXAPI connection_message_get_data(SPXCONNECTIONMESSAGEHANDLE hcm, uint8_t* data, uint32_t size)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto message = SpxGetPtrFromHandle<ISpxConnectionMessage>(hcm);

        auto buffer = message->GetBuffer();
        auto bufferSize = message->GetBufferSize();
        SPX_THROW_HR_IF(SPXERR_OUT_OF_RANGE, size > bufferSize);

        memcpy(data, buffer, size);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI_(uint32_t) connection_message_get_data_size(SPXCONNECTIONMESSAGEHANDLE hcm)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto message = SpxGetPtrFromHandle<ISpxConnectionMessage>(hcm);
        return message->GetBufferSize();
    }
    SPXAPI_CATCH_AND_RETURN(hr, 0);
}

SPXAPI connection_get_property_bag(SPXRECOHANDLE hconn, SPXPROPERTYBAGHANDLE* hpropbag)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !connection_handle_is_valid(hconn));
    return CSpxApiManager::QueryInterfaceAlwaysNoError<SPXRECOHANDLE, ISpxConnection, SPXPROPERTYBAGHANDLE, ISpxNamedProperties>(hconn, hpropbag);
}
