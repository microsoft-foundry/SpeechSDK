//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "http_request.h"
#include "http_response.h"
#include "service_helpers.h"
#include "../../../c_api/binding_callback_storage.h"
#include "../../../pal/include/guid.h"
#include "../../../common/include/handle_table.h"
#include <sstream>

using namespace Microsoft::CognitiveServices::Speech::Impl;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Helper function to convert HttpMethod to string
std::string HttpMethodToString(HttpMethod method)
{
    switch (method)
    {
    case HttpMethod::Get:
        return "GET";
    case HttpMethod::Post:
        return "POST";
    case HttpMethod::Put:
        return "PUT";
    case HttpMethod::Delete:
        return "DELETE";
    case HttpMethod::Patch:
        return "PATCH";
    default:
        return "GET";
    }
}

std::unique_ptr<ISpxHttpResponse> CSpxBindingBasedHttpRequest::SendRequest(
    HttpMethod method,
    const IHttpEndpointInfo& endpoint,
    const uint8_t* content,
    size_t contentSize,
    const ISpxHttpErrorHandler::ConstPtr& errorHandler)
{
    UNUSED(errorHandler);

    // Create event args for the request
    auto requestArgs = std::make_shared<CSpxHttpEventArgs>();

    // Generate a unique request ID
    std::string requestId = "http_" + PAL::GenerateGUID();
    requestArgs->SetStringValue("service.transport.http.response.id", requestId.c_str());

    // Populate the event args with request details
    PopulateEventArgs(requestArgs, method, endpoint, content, contentSize);

    // Signal the event to C# and wait for it to complete (synchronously)
    CallbackStorage::Instance().SignalSynchronously(requestArgs);

    // Return response created from data populated by C#
    auto response = PopulateResponse(requestArgs);
    return response;

}

// Populate event args directly
void CSpxBindingBasedHttpRequest::PopulateEventArgs(
    std::shared_ptr<CSpxHttpEventArgs> eventArgs,
    HttpMethod method,
    const IHttpEndpointInfo& endpoint,
    const uint8_t* content,
    size_t contentSize)
{

    eventArgs->SetStringValue("service.transport.http.event.type", "request");
    eventArgs->SetStringValue("service.transport.http.request.method", HttpMethodToString(method).c_str());
    eventArgs->SetStringValue("service.transport.http.request.uri", endpoint.EndpointUrl().c_str());

    // Prepare headers string and set individual headers
    std::string headers;

    // Add Host header
    headers += "Host\n";
    eventArgs->SetStringValue("service.transport.http.request.headers.Host", endpoint.Host().c_str());

    // Add Content-Length header
    headers += "Content-Length\n";
    eventArgs->SetStringValue("service.transport.http.request.headers.Content-Length", std::to_string(contentSize).c_str());

    // Add other headers
    for (const auto& header : endpoint.Headers())
    {
        headers += header.first + "\n";
        std::string headerName = "service.transport.http.request.headers." + header.first;
        eventArgs->SetStringValue(headerName.c_str(), header.second.c_str());
    }

    // Set all headers
    eventArgs->SetStringValue("service.transport.http.request.headers", headers.c_str());

    // Set content if available
    if (content != nullptr && contentSize > 0)
    {
       
        if (IsTextContent(endpoint))
        {
            std::string textContent(reinterpret_cast<const char*>(content), contentSize);
            eventArgs->SetStringValue("service.transport.http.request.content", textContent.c_str());

            // Explicitly set content length property
            eventArgs->SetStringValue("service.transport.http.request.content.length", 
                std::to_string(contentSize).c_str());
        }
        else
        {
            // For binary content, just set a flag for now
            // TODO: Implement binary content handling
            eventArgs->SetStringValue("service.transport.http.request.has_content", "true");
            SPX_TRACE_INFO("Binary content detected (length=%zu)", contentSize);
        }
    }
    else
    {
        SPX_TRACE_VERBOSE("No content provided for request");
    }
}

// Helper function to get default reason phrase for status code
static std::string GetDefaultReasonPhrase(int statusCode)
{
    switch (statusCode)
    {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default: return "Unknown Status";
    }
}

std::unique_ptr<ISpxHttpResponse> CSpxBindingBasedHttpRequest::PopulateResponse(
    std::shared_ptr<CSpxHttpEventArgs> responseArgs)
{
    auto response = std::make_unique<CSpxBindingBasedHttpResponse>();

    // Get status code (default to 500 if not found)
    std::string statusStr = responseArgs->GetStringValue("service.transport.http.response.status", "");
    

    try {
        // Try to parse the status code
        if (!statusStr.empty()) {
            response->m_statusCode = std::stoi(statusStr);
        }
        else {
            // No status code found - this suggests the C# handler didn't populate it
            SPX_TRACE_ERROR("No status code in response");
            response->m_statusCode = 500;
        }
    }
    catch (const std::exception& ex) {
        // Handle parsing errors
        SPX_TRACE_ERROR("Error parsing status code '%s': %s", statusStr.c_str(), ex.what());
        response->m_statusCode = 500;
    }

    // Get reason phrase with better fallback handling
    response->m_reasonPhrase = responseArgs->GetStringValue(
        "service.transport.http.response.reasonphrase",
        GetDefaultReasonPhrase(response->m_statusCode).c_str());

    // Check for exception
    std::string exception = responseArgs->GetStringValue("service.transport.http.response.exception", "");
    if (!exception.empty()) {
        SPX_TRACE_ERROR("HTTP request failed: %s", exception.c_str());
        response->m_responseHeaders["X-Error-Details"] = exception;
    }

    // Get headers
    std::string headers = responseArgs->GetStringValue("service.transport.http.response.headers", "");
    std::istringstream headerStream(headers);
    std::string headerName;
    while (std::getline(headerStream, headerName, '\n'))
    {
        if (!headerName.empty())
        {
            std::string headerKey = "service.transport.http.response.headers." + headerName;
            std::string headerValue = responseArgs->GetStringValue(headerKey.c_str(), "");
            response->m_responseHeaders[headerName] = headerValue;
        }
    }

    // Get content
    std::string contentText = responseArgs->GetStringValue("service.transport.http.response.content", "");
    if (!contentText.empty())
    {
        response->m_buffer = VariantValue::From(
            reinterpret_cast<const uint8_t*>(contentText.data()),
            contentText.size());
    }

    return response;
}

// Helper to determine if content is likely text
bool CSpxBindingBasedHttpRequest::IsTextContent(const IHttpEndpointInfo& endpoint) const
{
    // Check Content-Type header
    for (const auto& header : endpoint.Headers())
    {
        if (header.first == "Content-Type")
        {
            std::string contentType = header.second;
            return contentType.find("text/") == 0 ||
                contentType.find("application/json") != std::string::npos ||
                contentType.find("application/xml") != std::string::npos;
        }
    }
    return false;
}

std::unique_ptr<ISpxHttpResponse> CSpxBindingBasedHttpRequest::SendRequestStreamResponse(
    HttpMethod method,
    const IHttpEndpointInfo& endpoint,
    StreamedResponseDataHandler&& onDataCallback,
    const uint8_t* content,
    size_t contentSize,
    const ISpxHttpErrorHandler::ConstPtr& errorHandler)
{
    UNUSED(errorHandler);

    // Create event args for the request
    auto requestArgs = std::make_shared<CSpxHttpEventArgs>();

    SPX_TRACE_INFO("Sending streaming request: %s %s", 
        HttpMethodToString(method).c_str(), 
        endpoint.EndpointUrl().c_str());

    // Populate the event args with request details
    PopulateEventArgs(requestArgs, method, endpoint, content, contentSize);

    // For streaming requests, ensure text content is always set
    if (content != nullptr && contentSize > 0)
    {
        std::string textContent(reinterpret_cast<const char*>(content), contentSize);
        requestArgs->SetStringValue("service.transport.http.request.content", textContent.c_str());
        requestArgs->SetStringValue("service.transport.http.request.content.length",
            std::to_string(contentSize).c_str());
    }
    else
    {
        SPX_TRACE_VERBOSE("No content provided for streaming request");
    }

    // Create response and register callback
    auto sharedResponse = std::make_shared<CSpxBindingBasedHttpResponse>();
    sharedResponse->m_dataAvailableCallback = std::move(onDataCallback);

    // Register in handle table using ISpxHttpResponse interface 
    SPXEVENTHANDLE responseHandle = CSpxSharedPtrHandleTableManager::TrackHandle<ISpxHttpResponse, SPXEVENTHANDLE>(sharedResponse);

    // Store the handle as a string in the property bag
    std::string responseHandleStr = std::to_string(reinterpret_cast<uintptr_t>(responseHandle));
    requestArgs->SetStringValue("service.transport.http.response.handle", responseHandleStr.c_str());
    
    // Store the callback in our global map, keyed by the response pointer
    static std::map<void*, StreamedResponseDataHandler> callbackMap;
    callbackMap[sharedResponse.get()] = sharedResponse->m_dataAvailableCallback;

    // Signal to C# to handle the request
    CallbackStorage::Instance().SignalSynchronously(requestArgs);

    // Create a new response object to return
    auto finalResponse = PopulateResponse(requestArgs);
    
    return finalResponse;
}

}}}} // Microsoft::CognitiveServices::Speech::Impl


