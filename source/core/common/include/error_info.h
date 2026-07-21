//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license
// information.
//

#pragma once

#include "spxcore_common.h" // must include first
#include "ispxinterfaces.h"
#include "http_status_codes.h"
#include "interfaces/i_web_socket_state.h"
#include "recognition_status.h"

namespace Microsoft { namespace CognitiveServices { namespace Speech { namespace Impl {

  class ErrorInfo
    : public ISpxErrorInformation {
  private:
    std::string error_details;
    const CancellationErrorCode cancellation_code;
    const CancellationReason cancellation_reason;
    RetryMode retry_mode;
    const int category_code;
    const int status_code;

  public:
    ErrorInfo(
        std::string details,
        int categoryCode,
        int statusCode,
        CancellationErrorCode cancellationCode,
        CancellationReason reason,
        RetryMode retryMode)
        : error_details{ std::move(details) }, cancellation_code{ cancellationCode },
        cancellation_reason{ reason }, retry_mode{ retryMode }, category_code{ categoryCode },
        status_code{ statusCode }
    {
    }

    // Directly generates an error based on the provided cancellation code and error details,
    // needed when there's no other origination information to derive the error from.
    static std::shared_ptr<ISpxErrorInformation> FromExplicitError(
        CancellationErrorCode error,
        const std::string& errorDetails);

    // Generates an error based on a RecognitionStatus, as received from the SR engine adapter
    // layer.
    static std::shared_ptr<ISpxErrorInformation> FromRecognitionStatus(
        RecognitionStatus status,
        const std::string& errorDetails);

    // Generates an error based on a WebSocket error and/or disconnection, appending provided
    // details.
    static std::shared_ptr<ISpxErrorInformation> FromWebSocket(
        WebSocketError error,
        int status,
        const std::string& = "");
    static std::shared_ptr<ISpxErrorInformation> FromWebSocket(
        WebSocketError error,
        WebSocketDisconnectReason reason,
        const std::string& errorDetails = "")
    {
      return FromWebSocket(error, (int)reason, errorDetails);
    }

    // Generates an error based on the provided HTTP status code, prepending and appending (as
    // applicable) the provided message components.
    static std::shared_ptr<ISpxErrorInformation> FromHttpStatus(
        HttpStatusCode status,
        const std::string& reasonPhrase = "",
        const std::string& messagePrefix = "",
        const std::string& messageSuffix = "");

    // A less desirable factory method, this creates a new error as a copy of an existing error
    // with additional data appended to the message. This should be moved into a single-origination
    // and/or nested error model with future improvements.
    static std::shared_ptr<ISpxErrorInformation> FromErrorWithAppendedDetails(
        const std::shared_ptr<ISpxErrorInformation>& previousError,
        const std::string& extraDetails);

    // A less desirable factory method, this creates an error with minimal, standard information
    // (reporting a runtime error) with only the details/message customized.
    static std::shared_ptr<ISpxErrorInformation> FromRuntimeMessage(const std::string& message);

  // ISpxErrorInformation implementation
  public:
    // Gets the full message text associated with this error.
    const std::string& GetDetails() const final override { return error_details; }

    // Gets the public-facing error code enumeration value associated with this error.
    CancellationErrorCode GetCancellationCode() const final override { return cancellation_code; }

    // Gets the public-facing cancellation reason (usually "error") associated with the error.
    CancellationReason GetCancellationReason() const final override { return cancellation_reason; }

    // Gets the primary code associated with the error, e.g. the WebSocket status code.
    int GetCategoryCode() const final override { return category_code; }

    // When applicable, gets the secondary code associated with the error, e.g.the underlying
    // HTTP status code for a WebSocket error.
    int GetStatusCode() const final override { return status_code; }

    // The retry mode could be set by default based on the error type, http status code, etc.
    // you can overwrite the default retry mode by this method.
    void SetRetryMode(RetryMode retryMode) final override { retry_mode = retryMode; }

    // Gets the retry mode for this error, which defines how automatic retry attempts should be
    // treated when the error is encountered.
    RetryMode GetRetryMode() const final override { return retry_mode; }
  };

  // Get a system error message based on the error number.
  std::string GetSystemErrorMsg(int errorNum);

}}}} // namespace Microsoft::CognitiveServices::Speech::Impl
