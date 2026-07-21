//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <interfaces/types.h>
#include <interfaces/http_method.h>
#include <interfaces/i_http_endpoint_info.h>
#include <interfaces/ispx_http_response.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// An interface to implement a custom handler for networking failures
    /// </summary>
    SPX_INTERFACE(ISpxHttpErrorHandler)
    {
    public:
        /// <summary>
        /// Checks to see if the response indicates success
        /// </summary>
        /// <param name="response">The response to check</param>
        /// <returns>True if the response indicates success, false otherwise</returns>
        virtual bool IsSuccess(const ISpxHttpResponse* response) const = 0;

        /// <summary>
        /// Handles the result of sending the HTTP request for any errors. The result could indicate send failures
        /// such as failed DNS lookups, networking errors, etc...
        /// </summary>
        /// <param name="method">The HTTP method for your request</param>
        /// <param name="request">The request details</param>
        /// <param name="errorCode">The error code returned when attempting to send the request</param>
        /// <param name="additionalErrorCode">(Optional) Any additional error codes. Set to 0 if there is none</param>
        virtual void HandleSendResult(HttpMethod method, const IHttpEndpointInfo* request, int32_t errorCode, int32_t additionalErrorCode, const char *errorText) const = 0;

        /// <summary>
        /// Handles the result of sending the HTTP request for any errors. The result could indicate send failures
        /// such as failed DNS lookups, networking errors, etc...
        /// </summary>
        /// <param name="method">The HTTP method for your request</param>
        /// <param name="request">The request details</param>
        /// <param name="error">The error that occurred when attempting to send the request</param>
        virtual void HandleSendError(HttpMethod method, const IHttpEndpointInfo* request, const std::string& error) const = 0;

        /// <summary>
        /// Generates a message describing any errors that occurred while sending HTTP requests. In the case of
        /// success an empty string is returned
        /// </summary>
        /// <param name="method">The HTTP method for your request</param>
        /// <param name="request">The request details</param>
        /// <param name="error">The error that occurred when attempting to send the request</param>
        /// <returns>A message describing the error, or an empty string in the case of success</returns>
        virtual std::string GenerateSendErrorMessage(HttpMethod method, const IHttpEndpointInfo * request, const std::string& error) const = 0;

        /// <summary>
        /// Handles the HTTP response returned from the service for any error. For example you could examine the
        /// HTTP status code to see if it indicates success (2XX status codes), look into the response body for
        /// some error indications, etc...
        /// </summary>
        /// <param name="method">The HTTP method used to send the request</param>
        /// <param name="request">The request details</param>
        /// <param name="response">The response from the service</param>
        virtual void HandleResponse(HttpMethod method, const IHttpEndpointInfo* request, const ISpxHttpResponse* response) const = 0;

        /// <summary>
        /// Generates a message describing any errors in the HTTP response returned from the service. For
        /// example you could examine the HTTP status code to see if it indicates success (2XX status codes),
        /// look into the response body for some error indications, etc... In the case of success an empty
        /// string is returned
        /// </summary>
        //// <param name="method">The HTTP method used to send the request</param>
        /// <param name="request">The request details</param>
        /// <param name="response">The response from the service</param>
        /// <returns>A message describing the error, or an empty string in the case of success</returns>
        virtual std::string GenerateResponseErrorMessage(HttpMethod method, const IHttpEndpointInfo * request, const ISpxHttpResponse * response) const = 0;
    };

}}}}
