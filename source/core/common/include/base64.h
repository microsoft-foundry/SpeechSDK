//
// Copyright (c) Microsoft. All rights reserved.
// See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class Base64
    {
    public:
        /// <summary>
        /// Base 64 encodes the binary data
        /// </summary>
        /// <param name="data">Binary data to encode</param>
        /// <returns>Base 64 encoded string for the data</returns>
        /// <exception cref="std::invalid_argument">If the number of bytes to encode is too large</exception>
        /// <exception cref="std::bad_alloc">If we could not allocate enough bytes to store the base 64 encoded string</exception>
        static std::string Encode(const std::vector<uint8_t>& data);

        /// <summary>
        /// Base 64 encodes the binary data
        /// </summary>
        /// <param name="data">Binary data to encode</param>
        /// <param name="size">The size of the binary data to encode</param>
        /// <returns>Base 64 encoded string for the data</returns>
        /// <exception cref="std::invalid_argument">If the data pointer is null, or if the number of bytes to encode is too large</exception>
        /// <exception cref="std::bad_alloc">If we could not allocate enough bytes to store the base 64 encoded string</exception>
        static std::string Encode(const uint8_t* data, size_t size);

        /// <summary>
        /// Base 64 decodes the string ignoring all white space in the string
        /// </summary>
        /// <param name="base64">The base 64 encoded data to decode</param>
        /// <returns>The decoded data</returns>
        /// <exception cref="std::bad_alloc">If we could not allocate enough bytes to store the base 64 decoded data</exception>
        static std::vector<uint8_t> Decode(const std::string& base64);

        /// <summary>
        /// Base 64 decodes the characters ignoring all white space characters
        /// </summary>
        /// <param name="base64">The base 64 encoded data to decode</param>
        /// <param name="numChars">The number of characters to decode</param>
        /// <returns>The decoded data</returns>
        /// <exception cref="std::invalid_argument">If the base 64 pointer is null, or if the base 64 data is invalid</exception>
        /// <exception cref="std::bad_alloc">If we could not allocate enough bytes to store the base 64 decoded data</exception>
        static std::vector<uint8_t> Decode(const char* base64, size_t numChars);
    };

}}}}
