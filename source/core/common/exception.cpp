//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <iomanip>
#include <stdio.h>
#include "crt_abstractions.h"
#include "exception.h"
#include "debug_utils.h"
#include "handle_table.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    std::string stringify(SPXHR hr)
    {
        const char* error = "";

        switch (hr)
        {
#define _MAP_ENTRY(x) case x: error = #x; break;
            _MAP_ENTRY(SPXERR_NOT_IMPL)
            _MAP_ENTRY(SPXERR_UNINITIALIZED)
            _MAP_ENTRY(SPXERR_ALREADY_INITIALIZED)
            _MAP_ENTRY(SPXERR_UNHANDLED_EXCEPTION)
            _MAP_ENTRY(SPXERR_NOT_FOUND)
            _MAP_ENTRY(SPXERR_INVALID_ARG)
            _MAP_ENTRY(SPXERR_TIMEOUT)
            _MAP_ENTRY(SPXERR_ALREADY_IN_PROGRESS)
            _MAP_ENTRY(SPXERR_FILE_OPEN_FAILED)
            _MAP_ENTRY(SPXERR_UNEXPECTED_EOF)
            _MAP_ENTRY(SPXERR_INVALID_HEADER)
            _MAP_ENTRY(SPXERR_AUDIO_IS_PUMPING)
            _MAP_ENTRY(SPXERR_UNSUPPORTED_FORMAT)
            _MAP_ENTRY(SPXERR_ABORT)
            _MAP_ENTRY(SPXERR_MIC_NOT_AVAILABLE)
            _MAP_ENTRY(SPXERR_INVALID_STATE)
            _MAP_ENTRY(SPXERR_UUID_CREATE_FAILED)
            _MAP_ENTRY(SPXERR_SETFORMAT_UNEXPECTED_STATE_TRANSITION)
            _MAP_ENTRY(SPXERR_PROCESS_AUDIO_INVALID_STATE)
            _MAP_ENTRY(SPXERR_START_RECOGNIZING_INVALID_STATE_TRANSITION)
            _MAP_ENTRY(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE)
            _MAP_ENTRY(SPXERR_MIC_ERROR)
            _MAP_ENTRY(SPXERR_NO_AUDIO_INPUT)
            _MAP_ENTRY(SPXERR_UNEXPECTED_USP_SITE_FAILURE)
            _MAP_ENTRY(SPXERR_BUFFER_TOO_SMALL)
            _MAP_ENTRY(SPXERR_OUT_OF_MEMORY)
            _MAP_ENTRY(SPXERR_RUNTIME_ERROR)
            _MAP_ENTRY(SPXERR_INVALID_URL)
            _MAP_ENTRY(SPXERR_INVALID_REGION)
            _MAP_ENTRY(SPXERR_SWITCH_MODE_NOT_ALLOWED)
            _MAP_ENTRY(SPXERR_CHANGE_CONNECTION_STATUS_NOT_ALLOWED)
            _MAP_ENTRY(SPXERR_EXPLICIT_CONNECTION_NOT_SUPPORTED_BY_RECOGNIZER)
            _MAP_ENTRY(SPXERR_INVALID_HANDLE)
            _MAP_ENTRY(SPXERR_INVALID_RECOGNIZER)
            _MAP_ENTRY(SPXERR_OUT_OF_RANGE)
            _MAP_ENTRY(SPXERR_EXTENSION_LIBRARY_NOT_FOUND)
            _MAP_ENTRY(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE)
            _MAP_ENTRY(SPXERR_GSTREAMER_INTERNAL_ERROR)
            _MAP_ENTRY(SPXERR_CONTAINER_FORMAT_NOT_SUPPORTED_ERROR)
            _MAP_ENTRY(SPXERR_GSTREAMER_NOT_FOUND_ERROR)
            _MAP_ENTRY(SPXERR_UNSUPPORTED_API_ERROR)
            _MAP_ENTRY(SPXERR_UNEXPECTED_CONVERSATION_SITE_FAILURE)
            _MAP_ENTRY(SPXERR_CANCELED)
            _MAP_ENTRY(SPXERR_AUDIO_SYS_LIBRARY_NOT_FOUND)
            _MAP_ENTRY(SPXERR_LOUDSPEAKER_ERROR)
            _MAP_ENTRY(SPXERR_VAD_CANNOT_BE_USED_WITH_KEYWORD_RECOGNIZER)
            _MAP_ENTRY(SPXERR_COULD_NOT_CREATE_ENGINE_ADAPTER)
            _MAP_ENTRY(SPXERR_MAS_LIBRARY_NOT_FOUND)
            _MAP_ENTRY(AZAC_ERR_INPUT_FILE_SIZE_IS_ZERO_BYTES)
            _MAP_ENTRY(AZAC_ERR_FAILED_TO_OPEN_INPUT_FILE_FOR_READING)
            _MAP_ENTRY(AZAC_ERR_FAILED_TO_READ_FROM_INPUT_FILE)
            _MAP_ENTRY(AZAC_ERR_INPUT_FILE_TOO_LARGE)
            _MAP_ENTRY(AZAC_ERR_UNSUPPORTED_URL_PROTOCOL)
            _MAP_ENTRY(AZAC_ERR_EMPTY_NULLABLE)
            _MAP_ENTRY(AZAC_ERR_INVALID_MODEL_VERSION_FORMAT)
            _MAP_ENTRY(SPXERR_WAV_READER_FILE_OPEN_FAILED)
            _MAP_ENTRY(SPXERR_WAV_WRITER_FILE_OPEN_FAILED)
            _MAP_ENTRY(SPXERR_LOG_FILE_OPEN_FAILED)
            _MAP_ENTRY(SPXERR_MEMORY_LOG_FILE_OPEN_FAILED)
            _MAP_ENTRY(SPXERR_KEYWORD_MODEL_FILE_OPEN_FAILED)
            _MAP_ENTRY(SPXERR_VAD_MODEL_FILE_OPEN_FAILED)
#undef _MAP_ENTRY
        }

        char buffer[256];
        PAL::sprintf_s(buffer, sizeof(buffer), "0x%x (%s)", (unsigned int)hr, error);

        return buffer;
    }

    ExceptionWithCallStack::ExceptionWithCallStack(SPXHR error, size_t skipLevels/* = 0*/) :
        std::runtime_error("Exception with an error code: " + stringify(error)),
        m_callstack(Debug::GetCallStack(skipLevels + 1)),
        m_error(error)
    {
    }

    ExceptionWithCallStack::ExceptionWithCallStack(const std::string& message, SPXHR error/* = SPXERR_UNHANDLED_EXCEPTION*/, size_t skipLevels /*= 0*/) :
        std::runtime_error(message),
        m_callstack(Debug::GetCallStack(skipLevels + 1)),
        m_error(error)
    {
    }

    ExceptionWithCallStack::ExceptionWithCallStack(SPXHR error, const std::string& message, const std::string& callstack) :
        std::runtime_error(message),
        m_callstack(callstack),
        m_error(error)
    {
    }

    const char* ExceptionWithCallStack::GetCallStack() const
    {
        return m_callstack.c_str();
    }

    SPXHR ExceptionWithCallStack::GetErrorCode() const
    {
        return m_error;
    }

    void ThrowWithCallstack(SPXHR hr, size_t skipLevels/* = 0*/)
    {
        ExceptionWithCallStack ex(hr, skipLevels + 1);
        SPX_TRACE_ERROR("About to throw %s %s", ex.what(), ex.GetCallStack());
        throw ex;
    }

    void ThrowRuntimeError(const std::string& msg, size_t skipLevels/* = 0*/)
    {
        ExceptionWithCallStack ex("Runtime error: " + msg, SPXERR_RUNTIME_ERROR, skipLevels + 1);
        SPX_TRACE_ERROR("About to throw %s %s", ex.what(), ex.GetCallStack());
        throw ex;
    }

    void ThrowInvalidArgumentException(const std::string& msg, size_t skipLevels/* = 0*/)
    {
        ExceptionWithCallStack ex("Invalid argument exception: " + msg, SPXERR_INVALID_ARG, skipLevels + 1);
        SPX_TRACE_ERROR("About to throw %s %s", ex.what(), ex.GetCallStack());
        throw ex;
    }

    void ThrowLogicError(const std::string& msg, size_t skipLevels/* = 0*/)
    {
        ExceptionWithCallStack ex("Logic error: " + msg, SPXERR_RUNTIME_ERROR, skipLevels + 1);
        SPX_TRACE_ERROR("About to throw %s %s", ex.what(), ex.GetCallStack());
        throw ex;
    }

    SPXHR StoreException(ExceptionWithCallStack&& ex)
    {
        auto errorHandles = CSpxSharedPtrHandleTableManager::Get<ExceptionWithCallStack, SPXERRORHANDLE>();

        // NOTE: In some cases, the exception passed here has already been tracked with a handle. If we
        //       don't check, we end up with a double wrapped exception and a leaked handle.
        auto errorCode = ex.GetErrorCode();
        if (errorCode != SPXERR_UNHANDLED_EXCEPTION)
        {
            auto possibleHandle = reinterpret_cast<SPXERRORHANDLE>(errorCode);
            if (errorHandles->IsTracked(possibleHandle))
            {
                return errorCode;
            }
        }

        std::shared_ptr<ExceptionWithCallStack> handle(new ExceptionWithCallStack(std::move(ex)));
        return reinterpret_cast<SPXHR>(errorHandles->TrackHandle(handle));
    }

    SPXHR StoreException(const std::exception& ex)
    {
        auto errorHandles = CSpxSharedPtrHandleTableManager::Get<ExceptionWithCallStack, SPXERRORHANDLE>();
        std::shared_ptr<ExceptionWithCallStack> handle(new ExceptionWithCallStack(ex.what()));
        return reinterpret_cast<SPXHR>(errorHandles->TrackHandle(handle));
    }

} } } } // Microsoft::CognitiveServices::Speech::Impl
