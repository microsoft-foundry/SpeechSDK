#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(tts)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")


set(HEADERS
    "${INC_DIR}/speech_synthesis_api_factory.h"
    "${INC_DIR}/boundary_type_enum_helpers.h"
    )

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/create_module_object.h"
    "${SRC_DIR}/synthesizer.h"
    "${SRC_DIR}/synthesizer_connection.h"
    "${SRC_DIR}/synthesis_request.h"
    "${SRC_DIR}/synthesis_result.h"
    "${SRC_DIR}/synthesis_event_args.h"
    "${SRC_DIR}/speech_synthesis_metadata_event_args.h"
    "${SRC_DIR}/word_boundary_event_args.h"
    "${SRC_DIR}/viseme_event_args.h"
    "${SRC_DIR}/bookmark_event_args.h"
    "${SRC_DIR}/voices_list_result.h"
    "${SRC_DIR}/voice_info.h"
    "${SRC_DIR}/synthesizer_timeout_management.h"
    "${SRC_DIR}/synthesis_latency.h"
    )

set(SOURCES
    "${SRC_DIR}/speech_synthesis_api_factory.cpp"
    "${SRC_DIR}/create_module_object.cpp"
    "${SRC_DIR}/synthesizer.cpp"
    "${SRC_DIR}/synthesizer_connection.cpp"
    "${SRC_DIR}/synthesis_request.cpp"
    "${SRC_DIR}/synthesis_result.cpp"
    "${SRC_DIR}/synthesis_event_args.cpp"
    "${SRC_DIR}/word_boundary_event_args.cpp"
    "${SRC_DIR}/viseme_event_args.cpp"
    "${SRC_DIR}/bookmark_event_args.cpp"
    "${SRC_DIR}/voices_list_result.cpp"
    "${SRC_DIR}/voice_info.cpp"
    "${SRC_DIR}/synthesizer_timeout_management.cpp"
    "${SRC_DIR}/boundary_type_enum_helpers.cpp"
    )

set_source_groups()

# Workaround: Apple Clang 17 (Xcode 16.4) ICE (segfault) compiling this file
# at -O3 for arm64-apple-ios.  Drop to -O2 for this single TU.
if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang" AND CMAKE_SYSTEM_NAME MATCHES "iOS|Darwin")
  set_source_files_properties(
    "${SRC_DIR}/speech_synthesis_api_factory.cpp"
    PROPERTIES COMPILE_OPTIONS "-O2"
  )
endif()

add_library(tts STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(tts
    PRIVATE
        ${SRC_DIR}
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
        ${COMMON_GENERATED_INCLUDE}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (tts PROPERTIES
    FOLDER core/tts
)

target_link_libraries(tts PRIVATE interfaces common usp)

