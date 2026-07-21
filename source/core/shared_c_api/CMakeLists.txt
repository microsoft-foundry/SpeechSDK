#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(shared_c_api)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

set(HEADERS)

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    )

set(SOURCES
    "${SRC_DIR}/stdafx.cpp"
    "${SRC_DIR}/c_diagnostics.cpp"
    "${SRC_DIR}/c_error.cpp"
    "${SRC_DIR}/c_json.cpp"
    "${SRC_DIR}/c_property_bag.cpp"
    "${SRC_DIR}/c_pal.cpp"
    )

set_source_groups()

source_group("Shared C API headers" FILES ${SHARED_C_API_HEADERS})

add_library(shared_c_api OBJECT
    ${SHARED_C_API_HEADERS}
    ${HEADERS}
    ${HEADERS_PRIVATE}
    ${SOURCES}
    )

target_include_directories(shared_c_api
    PRIVATE
        ${CARBON_CXX_API}
        ${CARBON_C_API}
        ${CARBON_SHARED_C_API}
        ${CARBON_JSON_PARSER}
)

target_link_libraries(shared_c_api PRIVATE interfaces common logging)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (shared_c_api PROPERTIES
    FOLDER core
    COMPILE_DEFINITIONS "AZAC_CONFIG_EXPORTAPIS"
)