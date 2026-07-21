#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
cmake_minimum_required (VERSION 3.12)

project ("samples")

# Set Speech SDK header and library paths
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    # vcpkg: Use vcpkg-installed SDK
    set(SPEECHSDK_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
else()
    # non-vcpkg: Use local SDK directory
    set(SPEECHSDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SpeechSDK")
endif()

message(STATUS "SPEECHSDK_ROOT: ${SPEECHSDK_ROOT}")

set(SPEECHSDK_C_API "${SPEECHSDK_ROOT}/include/c_api")
set(SPEECHSDK_CXX_API "${SPEECHSDK_ROOT}/include/cxx_api")
set(SPEECHSDK_BIN_DIR "${SPEECHSDK_ROOT}/bin")
set(SPEECHSDK_LIB_DIR "${SPEECHSDK_ROOT}/lib")

# Include sub-projects
set(SDKDEMO "sdkdemo")
add_subdirectory(${SDKDEMO})
