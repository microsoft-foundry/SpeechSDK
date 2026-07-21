#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(networking)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/default_http_error_handler.h"
    "${INC_DIR}/http_endpoint_info.h"
    "${INC_DIR}/http_exception.h"
    "${INC_DIR}/http_headers.h"
    "${INC_DIR}/http_method_helpers.h"
    "${INC_DIR}/http_utils.h"
    "${INC_DIR}/queued_item.h"
    "${INC_DIR}/web_socket_enum_helpers.h"
    "${INC_DIR}/web_socket_message.h"
    "${INC_DIR}/web_socket_delegate_impl.h"
    "${INC_DIR}/web_socket_delegate_helper.h"
    "${INC_DIR}/web_socketInit_delegate_impl.h"
    "${INC_DIR}/web_socketInit_delegate_helper.h"
    "${INC_DIR}/web_socket_redirect_delegate.h"
    "${INC_DIR}/web_socket_queue_delegate.h"
    "${INC_DIR}/web_socket_url_cache.h"
)

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
)

set(SOURCES
    "${SRC_DIR}/default_http_error_handler.cpp"
    "${SRC_DIR}/http_endpoint_info.cpp"
    "${SRC_DIR}/http_exception.cpp"
    "${SRC_DIR}/http_method_helpers.cpp"
    "${SRC_DIR}/http_utils.cpp"
    "${SRC_DIR}/queued_item.cpp"
    "${SRC_DIR}/web_socket_enum_helpers.cpp"
    "${SRC_DIR}/web_socket_message.cpp"
    "${SRC_DIR}/web_socket_redirect_delegate.cpp"
    "${SRC_DIR}/web_socket_queue_delegate.cpp"
)

set_source_groups()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
        ${COMMON_GENERATED_INCLUDE}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER core/network
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        interfaces
        common
        network_util
)
