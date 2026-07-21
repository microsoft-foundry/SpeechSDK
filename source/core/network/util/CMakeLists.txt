#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(network_util)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/network/usp/header_builder.h"
    "${INC_DIR}/network/usp/header_utils.h"
    "${INC_DIR}/network/usp/header_view.h"
    "${INC_DIR}/network/usp/message_builder.h"
    "${INC_DIR}/network/usp/message_utils.h"
)

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/header_names.h"
)

set(SOURCES
    "${SRC_DIR}/header_builder.cpp"
    "${SRC_DIR}/header_utils.cpp"
    "${SRC_DIR}/header_view.cpp"
    "${SRC_DIR}/message_builder.cpp"
    "${SRC_DIR}/message_utils.cpp"
)

set_source_groups()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC network_types util common)

add_security_compile_flags(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${INC_DIR}
    PRIVATE
        ${CARBON_C_API}
        # This is needed as spxcore_common.h includes a CXX API header.
        ${CARBON_CXX_API}
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER core/network)
