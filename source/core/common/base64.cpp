//
// Copyright (c) Microsoft. All rights reserved.
// See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <array>
#include <cassert>
#include "base64.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    static constexpr const char* ERR_TOO_LARGE = "Binary data is too large to base 64 encode";
    static constexpr const char* ERR_INVALID_STRING = "Base 64 string is invalid";
    static constexpr const char* ERR_INVALID_LENGTH = "Base 64 string has incorrect length";
    static constexpr const char* ERR_NULL_POINTER = "Pointer passed to base 64 function cannot be null";

    static constexpr std::array<char, 64> ENCODE_BASE64_CHARS =
    {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
    };

    static constexpr uint8_t CODE_EQ = 64u;
    static constexpr uint8_t CODE_WHITE_SPACE = 65u;

    static constexpr uint32_t EMPTY_BLOCKCODE = 0xFF;

    static constexpr std::array<uint8_t, 256> DECODE_BASE64_CODES =
    {
        66, 66, 66, 66, 66, 66, 66, 66, 66, 65, 65, 65, 65, 65, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        65, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 62, 66, 66, 66, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 66, 66, 66, 64, 66, 66,
        66, 00,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 66, 66, 66, 66, 66,
        66, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 65, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        65, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66
    };

    /// <summary>
    /// Gets the exact number of characters we need to encode the specified number of bytes
    /// in base 64 notation
    /// </summary>
    /// <param name="size">The size of the input data</param>
    /// <returns>The number of characters needed</returns>
    static inline size_t GetEncodedSize(size_t size)
    {
        size_t base64Size = ((size + 2) / 3) * 4;

        // did we wrap around indicating the byte data size was too large to base 64 encode?
        if (size > base64Size)
        {
            throw std::invalid_argument(ERR_TOO_LARGE);
        }

        return base64Size;
    }

    /// <summary>
    /// Gets the maximum number of bytes the corresponding decoded base 64 data could be. The actual number
    /// of decoded bytes may be less since we can ignore padding '=', and any white space
    /// </summary>
    /// <param name="size">The number of base 64 encoded characters</param>
    /// <returns>The corresponding maximum size of the decoded data</returns>
    static inline size_t GetMaxDecodedSize(size_t size)
    {
        size_t decodedSize = size / 4 * 3;
        return decodedSize;
    }

    std::string Base64::Encode(const uint8_t* data, size_t size)
    {
        // Code is a modified version of the C# Convert.ToBase64 function with optimizations for C++:
        // https://referencesource.microsoft.com/#mscorlib/system/convert.cs,6089a930276e17f5

        if (size == 0)
        {
            return std::string{};
        }
        else if (data == nullptr)
        {
            throw std::invalid_argument(ERR_NULL_POINTER);
        }

        // To improve performance, we pre-allocate the string upfront, and then we directly set the characters at each position
        size_t base64Size = GetEncodedSize(size);
        std::string base64(base64Size, '\0');
        char* psBase64 = &base64[0];

        size_t lengthmod3 = size % 3;
        size_t calcLength = size - lengthmod3;
        const uint8_t* stopAt = data + calcLength;
        const uint8_t* endAt = data + size;

        // Convert three bytes at a time to base64 notation. This will consume 4 chars.
        while (data < stopAt)
        {
            *psBase64++ = ENCODE_BASE64_CHARS[(data[0] & 0xfcu) >> 2];
            *psBase64++ = ENCODE_BASE64_CHARS[((data[0] & 0x03u) << 4) | ((data[1] & 0xf0u) >> 4)];
            *psBase64++ = ENCODE_BASE64_CHARS[((data[1] & 0x0fu) << 2) | ((data[2] & 0xc0u) >> 6)];
            *psBase64++ = ENCODE_BASE64_CHARS[(data[2] & 0x3fu)];
            
            data += 3;
        }

        switch (lengthmod3)
        {
        case 2: // One character padding needed
#ifdef _MSC_VER
    // Suppress nonsensical PREfast 'error' about condition not being true
    #pragma warning(push)
    #pragma warning(suppress: 28020)
#endif
            *psBase64++ = ENCODE_BASE64_CHARS[(data[0] & 0xfcu) >> 2];
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
            *psBase64++ = ENCODE_BASE64_CHARS[((data[0] & 0x03u) << 4) | ((data[1] & 0xf0u) >> 4)];
            *psBase64++ = ENCODE_BASE64_CHARS[(((size_t)data[1]) & 0x0fu) << 2]; // size_t cast to make PREfast happy
            *psBase64++ = '='; // Pad

            data += 2;
            break;

        case 1: // Two character padding needed
            *psBase64++ = ENCODE_BASE64_CHARS[(data[0] & 0xfcu) >> 2];
            *psBase64++ = ENCODE_BASE64_CHARS[(((size_t)data[0]) & 0x03u) << 4]; // size_t cast to make PREfast happy
            *psBase64++ = '='; // Pad
            *psBase64++ = '='; // Pad

            data += 1;
            break;
        }

        // sanity checks:
        //  - Did we write the correct number of base 64 characters?
        //  - Did we consume all the bytes in the data?
        assert(psBase64 == (&base64[0] + base64Size));
        assert(data == endAt);
        (void)endAt; // for release builds

        return base64;
    }

    std::string Base64::Encode(const std::vector<uint8_t>& data)
    {
        return Encode(data.data(), data.size());
    }

    std::vector<uint8_t> Base64::Decode(const char* base64, size_t numChars)
    {
        // Code is a modified version of Convert.FromBase64String with optimizations for C++:
        // https://referencesource.microsoft.com/#mscorlib/system/convert.cs,a7523aa4c36a3be4
        if (numChars == 0)
        {
            return std::vector<uint8_t>{};
        }
        else if (base64 == nullptr)
        {
            throw std::invalid_argument(ERR_NULL_POINTER);
        }

        // To improve performance, we pre-allocate the vector upfront, and then we directly set
        // the decoded bytes at each index. Since we may over allocate here, we also need to
        // resize the vector at the end as well
        size_t maxDecodedSize = GetMaxDecodedSize(numChars);
        std::vector<uint8_t> binaryData(maxDecodedSize);
        uint8_t* pBinaryData = binaryData.data();

        const char* pEndInput = base64 + numChars;
        uint8_t code;

        // This 4-byte integer will contain the 4 codes of the current 4-char group.
        // Each char codes for 6 bits = 24 bits.
        // The remaining byte will be FF, we use it as a marker when 4 chars have been processed.
        uint32_t currBlockCodes = EMPTY_BLOCKCODE;

        while (true)
        {
            // break when done:
            if (base64 >= pEndInput)
            {
                goto _AllInputConsumed;
            }

            // Get current code
            code = DECODE_BASE64_CODES[static_cast<uint8_t>(*base64++)];
            if (code < CODE_EQ)
            {
                // OK, we got the code. Save it:
                currBlockCodes = (currBlockCodes << 6) | code;

                // Last bit in currBlockCodes will be on after in shifted right 4 times:
                if ((currBlockCodes & 0x80000000u) != 0u)
                {
                    *pBinaryData++ = static_cast<uint8_t>(currBlockCodes >> 16);
                    *pBinaryData++ = static_cast<uint8_t>(currBlockCodes >> 8);
                    *pBinaryData++ = static_cast<uint8_t>(currBlockCodes);

                    currBlockCodes = EMPTY_BLOCKCODE;
                }
            }
            else if (code == CODE_WHITE_SPACE)
            {
                // ignore white space
                continue;
            }
            else if (code == CODE_EQ)
            {
                // we've reached the padding characters break out of this loop
                break;
            }
            else
            {
                // detected invalid characters in the string
                throw std::invalid_argument(ERR_INVALID_STRING);
            }
        }

        // We only break out of the loop and get here if we hit an '=':
        assert(code == CODE_EQ);

        // Code is zero for trailing '=':
        currBlockCodes <<= 6;
        code = 1; // code is now used to count the number of '=' found

        // we can have at most one more '=' so let's find it
        while (base64 < pEndInput)
        {
            switch (DECODE_BASE64_CODES[static_cast<uint8_t>(*base64++)])
            {
            case CODE_WHITE_SPACE:
                // ignore
                break;

            case CODE_EQ:
                currBlockCodes <<= 6;
                code++;
                break;

            default:
                // Only '=' or whitespace is allowed now
                throw std::invalid_argument(ERR_INVALID_STRING);
            }
        }

        if (code > 2)
        {
            // more than two '=' equals detected
            throw std::invalid_argument(ERR_INVALID_STRING);
        }

        if ((currBlockCodes & 0x80000000) == 0u)
        {
            // The '=' did not complete a 4-group. The input must be bad:
            throw std::invalid_argument(ERR_INVALID_LENGTH);
        }

        // We are good, store bytes form this past group
        *pBinaryData++ = static_cast<uint8_t>(currBlockCodes >> 16);
        if (code == 1)
        {
            *pBinaryData++ = static_cast<uint8_t>(currBlockCodes >> 8);
        }

        currBlockCodes = EMPTY_BLOCKCODE;

        // We get here either from above or by jumping out of the loop:
_AllInputConsumed:
        if (currBlockCodes != EMPTY_BLOCKCODE)
        {
            // The last block of chars has less than 4 items
            throw std::invalid_argument(ERR_INVALID_LENGTH);
        }

        // finally since we may have over allocated the vector, resize to the actual number of bytes
        // decoded
        assert(pBinaryData <= binaryData.data() + maxDecodedSize);
        binaryData.resize(pBinaryData - binaryData.data());

        return binaryData;
    }

    std::vector<uint8_t> Base64::Decode(const std::string& base64)
    {
        return Decode(base64.data(), base64.size());
    }

}}}}
