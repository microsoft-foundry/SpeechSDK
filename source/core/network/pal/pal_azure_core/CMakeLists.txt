#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(pal_azure_core)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
)

set(HEADERS_PRIVATE
    "${SRC_DIR}/http_platform_impl.h"
    "${SRC_DIR}/http_request.h"
    "${SRC_DIR}/http_response.h"
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/web_socket.h"
)

set(SOURCES
    "${SRC_DIR}/helpers/az_buffer_helper.cpp"
    "${SRC_DIR}/helpers/az_lock_helper.cpp"
    "${SRC_DIR}/helpers/az_strings_helper.cpp"
    "${SRC_DIR}/helpers/az_threadapi_helper.cpp"
    "${SRC_DIR}/http_platform_impl.cpp"
    "${SRC_DIR}/http_request.cpp"
    "${SRC_DIR}/http_response.cpp"
    "${SRC_DIR}/web_socket.cpp"
    "${SRC_DIR}/pal_create_module_object.cpp"
)

set_source_groups()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PUBLIC "NETWORKING_AZURE_CORE")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set(COMPILE_TIME_DEFINITIONS
    PAL_CONFIG_EXPORTAPIS
)

if(WIN32 AND NOT (CARBONSDK_OS STREQUAL "WindowsUwp"))
    list(APPEND COMPILE_TIME_DEFINITIONS BUILD_TRANSPORT_WINHTTP_ADAPTER)
else()
    list(APPEND COMPILE_TIME_DEFINITIONS BUILD_CURL_HTTP_TRANSPORT_ADAPTER)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER core/shared
    COMPILE_DEFINITIONS "${COMPILE_TIME_DEFINITIONS}"
)

set(ADDITIONAL_LIBS
    #TODO set your base libraries here that are required across all platforms
)

# Because of the special way we create and link iOS test projects, we need to manually specify all necessary
# static libraries to link against
list(APPEND IOS_ADDITIONAL_STATIC_LIBS
    $<TARGET_FILE:${PROJECT_NAME}>
    # you can add any additional full paths to static libraries here as well
)

if(NOT WIN32)
  find_package(Threads REQUIRED)
  list(APPEND ADDITIONAL_LIBS Threads::Threads)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE azure-core
    PRIVATE interfaces
    PRIVATE common
    PRIVATE networking
    PUBLIC network_pal
    PUBLIC ${ADDITIONAL_LIBS}
)

# CMake copies by value into child scopes. Explicitly update the parent scope to ensure other projects
# see the additional libs
if(NOT BUILD_CORE_ONLY)
  message(STATUS "IOS_ADDITIONAL_STATIC_LIBS=${IOS_ADDITIONAL_STATIC_LIBS}")
endif()
set(IOS_ADDITIONAL_STATIC_LIBS ${IOS_ADDITIONAL_STATIC_LIBS} PARENT_SCOPE)
