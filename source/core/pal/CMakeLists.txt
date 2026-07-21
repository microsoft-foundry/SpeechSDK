#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(pal)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

if(WIN32)
    set(PAL_SRC "${SRC_DIR}/windows")
    list(APPEND PAL_LIBS rpcrt4.lib Ole32.lib)
elseif(ANDROID)
    set(PAL_SRC "${SRC_DIR}/android")
elseif(EMSCRIPTEN)
    set(PAL_SRC "${SRC_DIR}/linux")
elseif(APPLE)
    set(PAL_SRC "${SRC_DIR}/apple")
    FIND_LIBRARY(CORE_FOUNDATION CoreFoundation REQUIRED)
    list(APPEND PAL_LIBS ${CORE_FOUNDATION})
    if(NATIVE_NETWORKING_IMPL STREQUAL "Azure_Core")
        find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
        list(APPEND PAL_LIBS ${FOUNDATION_LIBRARY})
    endif()
elseif(CARBONSDK_OS STREQUAL "Linux")
    FIND_LIBRARY(LIB_UUID uuid)
    if (NOT LIB_UUID)
        message(FATAL_ERROR "Could not find libuuid. Please make sure you have it installed")
    endif()
    set(PAL_SRC "${SRC_DIR}/linux")
    list(APPEND PAL_LIBS uuid)
else()
    message(FATAL_ERROR "Platform specific UUID generation for '${CARBONSDK_OS}' is not implemented.")
endif()

set(SOURCES
    "${SRC_DIR}/crt_abstractions.cpp"
    "${PAL_SRC}/environment.cpp"
    "${PAL_SRC}/guid.cpp"
)

set(HEADERS
    "${INC_DIR}/az_buffer_helper.h"
    "${INC_DIR}/az_lock_helper.h"
    "${INC_DIR}/az_strings_helper.h"
    "${INC_DIR}/az_threadapi_helper.h"
    "${INC_DIR}/crt_abstractions.h"
    "${INC_DIR}/guid.h"
)

set(HEADERS_PRIVATE
    "${PAL_SRC}/environment.h"
)

set_source_groups()

add_library(pal STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(pal
    PRIVATE
        ${PAL_SRC}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (pal PROPERTIES
    FOLDER core/shared
)

target_link_libraries(pal
    PUBLIC ${PAL_LIBS})
