//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <cstdint>
#include <string>
#include <memory>

#include "interfaces/base.h"
#include "interfaces/utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxReadWriteBuffer)
{
    public:

    virtual size_t GetSize() const = 0;
    virtual uint64_t GetInitPos() const = 0;
    virtual std::string GetName() const = 0;

    virtual void Write(const void* data, size_t dataSizeInBytes, size_t* bytesWritten = nullptr) = 0;
    virtual size_t WriteAtBytePos(uint64_t pos, const void* data, size_t dataSizeInBytes) = 0;
    virtual uint64_t GetWritePos() const = 0;

    virtual void Read(void* data, size_t dataSizeInBytes, size_t* bytesRead = nullptr) = 0;
    virtual uint64_t GetReadPos() const = 0;
    virtual uint64_t ResetReadPos() = 0;

    virtual void ReadAtBytePos(uint64_t pos, void* data, size_t dataSizeInBytes, size_t* bytesRead = nullptr) = 0;

    virtual std::shared_ptr<uint8_t> ReadShared(size_t dataSizeInBytes, size_t* bytesRead = nullptr) = 0;
    virtual std::shared_ptr<uint8_t> ReadSharedAtBytePos(uint64_t pos, size_t dataSizeInBytes, size_t* bytesRead = nullptr) = 0;

    template <class T>
    std::shared_ptr<T> ReadShared(size_t dataSizeInBytes, size_t* bytesRead = nullptr)
    {
        auto shared = ReadShared(dataSizeInBytes, bytesRead);
        return SpxReinterpretPointerCast<T>(shared);
    }

    template <class T>
    std::shared_ptr<T> ReadSharedAtBytePos(uint64_t pos, size_t dataSizeInBytes, size_t* bytesRead = nullptr)
    {
        auto shared = ReadSharedAtBytePos(pos, dataSizeInBytes, bytesRead);
        return SpxReinterpretPointerCast<T>(shared);
    }

    uint64_t GetBytesReadReady() { return GetWritePos() - GetReadPos(); }

    // TODO: more checks on pos
    uint64_t GetBytesReadReadyAtPos(uint64_t pos) { return GetWritePos() - pos; }

    // Free space currently available to write (capacity minus unread data). Mirror of GetBytesReadReady().
    uint64_t GetBytesWriteReady() { return GetSize() - (GetWritePos() - GetReadPos()); }

};

} } } }
