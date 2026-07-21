#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(tts_cloud)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/cloud_tts_engine_adapter.h"
    "${INC_DIR}/rest_tts_engine_adapter.h"
    "${INC_DIR}/usp_tts_engine_adapter.h"
)

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/create_module_object.h"
    )

set(SOURCES
    "${SRC_DIR}/create_module_object.cpp"
    "${SRC_DIR}/cloud_tts_engine_adapter.cpp"
    "${SRC_DIR}/rest_tts_engine_adapter.cpp"
    "${SRC_DIR}/usp_tts_engine_adapter.cpp"
    )

set_source_groups()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(${PROJECT_NAME}
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

set_target_properties (${PROJECT_NAME} PROPERTIES
    FOLDER core/tts
)

target_link_libraries(${PROJECT_NAME} interfaces usp common tts)
