//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <http_exception.h>
#include <http_platform.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    static std::string GenerateDefaultMessage(int32_t result, int httpStatusCode, const char *customMessage)
    {
        if (result != HttpException::RESULT_NO_ERROR)
        {
            std::string error("Failed with error: ");
            error += customMessage;
            return error;
        }
        else
        {
            std::string error("Failed with HTTP ");
            error += std::to_string(httpStatusCode);
            return error;
        }
    }

    const int32_t HttpException::RESULT_NO_ERROR = 0;

    HttpException::HttpException(int32_t result) :
        HttpException(result, (HttpStatusCode)0, GenerateDefaultMessage(result, 0, ""))
    {
    }

    HttpException::HttpException(int32_t result, const char* customMessage) :
        HttpException(result, (HttpStatusCode)0, GenerateDefaultMessage(result, 0, customMessage))
    {}

    HttpException::HttpException(int32_t result, const std::string& msg) :
        HttpException(result, (HttpStatusCode)0, msg)
    {}

    HttpException::HttpException(HttpStatusCode httpStatusCode, const char* customMessage) :
        HttpException(RESULT_NO_ERROR, httpStatusCode, GenerateDefaultMessage(RESULT_NO_ERROR, (int)httpStatusCode, customMessage))
    {}

    HttpException::HttpException(HttpStatusCode httpStatusCode, const std::string & msg) :
        HttpException(RESULT_NO_ERROR, httpStatusCode, msg)
    {}

    HttpException::HttpException(int32_t result, HttpStatusCode httpStatusCode, const char *msg)
        : std::runtime_error(msg), m_result(result), m_statusCode(httpStatusCode)
    {}

    HttpException::HttpException(int32_t result, HttpStatusCode httpStatusCode, const std::string& msg)
        : std::runtime_error(msg), m_result(result), m_statusCode(httpStatusCode)
    {}

    int32_t HttpException::result() const { return m_result; }
    HttpStatusCode HttpException::statusCode() const { return m_statusCode; }

} } } }
