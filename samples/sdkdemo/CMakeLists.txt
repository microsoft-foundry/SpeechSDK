#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
add_executable(${SDKDEMO} "sdkdemo.cpp")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

target_include_directories(${SDKDEMO}
    PRIVATE
    "${SPEECHSDK_C_API}"
    "${SPEECHSDK_CXX_API}"
)

find_library(SPEECHSDK_CORE_LIB
    NAMES Microsoft.CognitiveServices.Speech.core
    PATHS "${SPEECHSDK_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(SPEECHSDK_CORE_LIB)
    target_link_libraries(${SDKDEMO} PRIVATE "${SPEECHSDK_CORE_LIB}")
else()
    message(FATAL_ERROR "Speech SDK libraries not found!")
endif()

if(APPLE)
    set_target_properties(${SDKDEMO} PROPERTIES BUILD_RPATH "@loader_path")
elseif(UNIX)
    set_target_properties(${SDKDEMO} PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

# Copy SDK shared libraries to the same directory where the generated executable resides
# On Windows, shared libraries are in bin/ directory; on Unix platforms, in lib/
if(WIN32)
    file(GLOB SPEECHSDK_SHARED_LIBS "${SPEECHSDK_BIN_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
    file(GLOB SPEECHSDK_SHARED_LIBS "${SPEECHSDK_LIB_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

add_custom_command(
    TARGET ${SDKDEMO}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SPEECHSDK_SHARED_LIBS} "$<TARGET_FILE_DIR:${SDKDEMO}>/"
)
