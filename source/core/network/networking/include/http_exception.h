//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <http_status_codes.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class HttpException : public std::runtime_error
    {
    public:
        static const int32_t RESULT_NO_ERROR;

        explicit HttpException(int32_t result);
        explicit HttpException(int32_t result, const char* msg);
        explicit HttpException(int32_t result, const std::string& msg);

        explicit HttpException(HttpStatusCode httpStatusCode);
        explicit HttpException(HttpStatusCode httpStatusCode, const char* msg);
        explicit HttpException(HttpStatusCode httpStatusCode, const std::string& msg);

        explicit HttpException(int32_t result, HttpStatusCode httpStatusCode, const char *msg);
        explicit HttpException(int32_t result, HttpStatusCode httpStatusCode, const std::string& msg);

        virtual int32_t result() const;
        virtual HttpStatusCode statusCode() const;

    protected:
        int32_t m_result;
        HttpStatusCode m_statusCode;
    };

} } } } // Microsoft::CognitiveServices::Speech::Impl
