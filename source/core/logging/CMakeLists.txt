#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(logging)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${SRC_DIR}/stdafx.h"
    "${INC_DIR}/console_logger.h"
    "${INC_DIR}/file_logger.h"
    "${INC_DIR}/trace_message.h"
    "${INC_DIR}/event_logger.h"
    "${INC_DIR}/event_source_logger.h"
    "${INC_DIR}/fatal_exit_monitor.h"
    "${INC_DIR}/memory_logger.h"
    "${INC_DIR}/log_utils.h"
    "${INC_DIR}/log_config.h"
    )

set(SOURCES
    "${SRC_DIR}/console_logger.cpp"
    "${SRC_DIR}/file_logger.cpp"
    "${SRC_DIR}/trace_message.cpp"
    "${SRC_DIR}/event_logger.cpp"
    "${SRC_DIR}/event_source_logger.cpp"
    "${SRC_DIR}/fatal_exit_monitor.cpp"
    "${SRC_DIR}/memory_logger.cpp"
    "${SRC_DIR}/log_utils.cpp"
    "${SRC_DIR}/log_config.cpp"
    )

# make this the last source file to avoid link warning LNK4221
list(APPEND SOURCES "${SRC_DIR}/stdafx.cpp")

set_source_groups()

add_library(logging STATIC ${HEADERS} ${SOURCES})

target_include_directories(logging
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
        ${COMMON_GENERATED_INCLUDE}
    PUBLIC
        ${INC_DIR}
)

target_link_libraries(logging PRIVATE interfaces pal common)

add_security_compile_flags(${PROJECT_NAME})

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    # Need C++17 for WinRT API access via cppwinrt.
    set_target_properties(logging PROPERTIES CXX_STANDARD 17)
    target_compile_definitions(logging PUBLIC _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

set_target_properties(logging PROPERTIES
    FOLDER core/shared
)
