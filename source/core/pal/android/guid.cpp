//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "guid.h"
#include <array>
#include <random>

#include "environment.h"
#include "crt_abstractions.h"

namespace PAL
{

template<typename T>
T* throw_if_null(T* ptr, const char* error)
{
    if (ptr == nullptr)
    {
        throw std::runtime_error{ error };
    }
    return ptr;
}

constexpr size_t UUID_BYTES = 16;

std::string GenerateGUID()
{
    if (GetVM() == nullptr)
    {
        /**
         * This is a workaround for when the library is used through pinvoke as JNI_OnLoad doesn't get called.
         * The proper fix is to use something like
         * https://docs.microsoft.com/dotnet/api/java.lang.javasystem.loadlibrary?view=xamarin-android-sdk-9
         * to force the interop layer to initialized the JNI environment. Until we have that we are falling back to the
         * insecure guid generation
         */
        
        std::array<uint8_t, UUID_BYTES> randomBytes;

        // This section is copied from the Azure C Shared library with some tweaks to further reduce collisions:
        // https://github.com/Azure/azure-c-shared-utility/blob/51555845f446a7f0be4e3442a321c558af04910b/adapters/uniqueid_stub.c
        // 
        // The chance for collisions will vary on how good the std::random_device implementation is. On Windows 10 this seemed to
        // be somewhere between 1/32000 to better than 1/100000. The original Azure C shared version was worse at a low value of
        // 1/3300 even with tweak to set srand() increase randomness. That was due to casting away some of the randomness to fit
        // into uint8_t
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> dist(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());

            static_assert(UUID_BYTES % sizeof(uint32_t) == 0, "Your UUID size is not a perfect multiple of the uint32_t size on your platform");

            for (size_t i = 0; i < randomBytes.size(); i += sizeof(uint32_t))
            {
                uint32_t random = dist(gen);
                memcpy(&randomBytes[i], &random, sizeof(uint32_t));
            }
            
            // Stick in the version field for random uuid.
            randomBytes[6] &= 0x0f; //clear the bit field
            randomBytes[6] |= 0x40; //set the ones we care about

            // Stick in the variant field for the random uuid.
            randomBytes[8] &= 0x3f; // Clear
            randomBytes[8] |= 0x80; // Set
        }

        std::string uuid(PAL::UUID_LENGTH, '\0');
        PAL::sprintf_s(const_cast<char *>(uuid.c_str()),
            uuid.size() + 1, // +1 for terminating \0 that the std::string code automatically adds
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            randomBytes[0], randomBytes[1], randomBytes[2], randomBytes[3],
            randomBytes[4], randomBytes[5], randomBytes[6], randomBytes[7],
            randomBytes[8], randomBytes[9], randomBytes[10], randomBytes[11],
            randomBytes[12], randomBytes[13], randomBytes[14], randomBytes[15]);

        return uuid;

    }
    return RunOnEnv([](JNIEnv* env)
    {
        /**
         *  This is equivalent to the following java code:
         *
         *  var uuid = java.util.UUID.randomUUID();
         *  return uuid.toString();
         *
         *  With code added to do the UTF-16 -> UTF-8 conversion.
         */
        env->PushLocalFrame(16);
        auto cl = throw_if_null(env->FindClass("java/util/UUID"), "Can't find UUID class.");
        auto randomUUID = throw_if_null(env->GetStaticMethodID(cl, "randomUUID", "()Ljava/util/UUID;"), "Can't find static method \"UUID.randomUUID()\"");
        auto toString = throw_if_null(env->GetMethodID(cl, "toString", "()Ljava/lang/String;"), "Can't find method \"UUID.toString()\"");
        auto uuidObj = throw_if_null(env->CallStaticObjectMethod(cl, randomUUID), "Problem calling \"UUID.randomUUID()\"");
        auto uuidStr = static_cast<jstring>(throw_if_null(env->CallObjectMethod(uuidObj, toString), "Problem calling \"UUID.toString()\""));
        auto uuidUTFStr = env->GetStringUTFChars(uuidStr, 0);
        std::string uuid{ uuidUTFStr };
        env->DeleteLocalRef(uuidObj);
        env->ReleaseStringUTFChars(uuidStr, uuidUTFStr);
        env->PopLocalFrame(nullptr);
        return uuid;
    });
}

} // PAL
