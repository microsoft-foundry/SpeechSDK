#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(factory_speech)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${SRC_DIR}/stdafx.h"
    )

set(SOURCES
    "${SRC_DIR}/create_module_object.cpp"
    )

set_source_groups()

add_library(factory_speech STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(factory_speech
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (factory_speech PROPERTIES
    FOLDER core
)

target_link_libraries(factory_speech
                      PRIVATE interfaces
                      PRIVATE sr
                      PRIVATE tts
                      PRIVATE audio
                      PRIVATE usp
                      PRIVATE tts_cloud
                      PRIVATE common
                      PRIVATE data
                      PRIVATE network_pal)
