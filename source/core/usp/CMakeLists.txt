#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(usp)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/usp.h"
    "${INC_DIR}/usp_callbacks.h"
    "${INC_DIR}/usp_client_configuration.h"
    "${INC_DIR}/usp_connection.h"
    "${INC_DIR}/usp_endpoint.h"
    "${INC_DIR}/usp_enums.h"
    "${INC_DIR}/usperror.h"
    "${INC_DIR}/uspmessages.h"
    "${INC_DIR}/usp_message.h"
    "${INC_DIR}/usp_text_message.h"
    "${INC_DIR}/usp_binary_message.h"
    )

set(HEADERS_PRIVATE
    "${SRC_DIR}/create_module_object.h"
    "${SRC_DIR}/uspcommon.h"
    "${SRC_DIR}/uspinternal.h"
    "${SRC_DIR}/usp_metrics.h"
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/uspimpl.h"
    "${SRC_DIR}/usp_web_socket.h"
    "${SRC_DIR}/usp_constants.h"
    )

set(SOURCES
    "${SRC_DIR}/create_module_object.cpp"
    "${SRC_DIR}/usp_metrics.cpp"
    "${SRC_DIR}/usp_connection.cpp"
    "${SRC_DIR}/uspimpl.cpp"
    "${SRC_DIR}/usp_web_socket.cpp"
    "${SRC_DIR}/usp_message.cpp"
    "${SRC_DIR}/usp_text_message.cpp"
    "${SRC_DIR}/usp_binary_message.cpp"
    )

set_source_groups()

add_library(usp STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(usp
    PRIVATE
        ${SRC_DIR}
        ## TODO: These four should also come from target_link_libraries
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (usp PROPERTIES
    FOLDER core
)

target_link_libraries(usp interfaces common networking)
