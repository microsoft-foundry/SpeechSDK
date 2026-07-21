#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
add_subdirectory(external)

# azure-c-shared resets this to ONLY, needs to be BOTH
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)

message(STATUS "Including Azure C Shared external libraries from ${AZURE_SHARED_UTIL_INCLUDE}")
include_directories(${AZURE_SHARED_UTIL_INCLUDE})

add_subdirectory(interfaces)
add_subdirectory(pal)
add_subdirectory(common)
add_subdirectory(logging)
add_subdirectory(factory)
add_subdirectory(factory_speech)
add_subdirectory(audio)
add_subdirectory(network)
add_subdirectory(usp)
add_subdirectory(c_api)
add_subdirectory(sr)
add_subdirectory(tts)
if(NOT BUILD_CORE_ONLY)
  add_subdirectory(tts_mock)
endif()
add_subdirectory(tts_cloud)
if(NOT BUILD_CORE_ONLY)
  add_subdirectory(mock)
endif()
add_subdirectory(libcarbon)
add_subdirectory(data)
add_subdirectory(shared_c_api)
add_subdirectory(util)
add_subdirectory(types)
