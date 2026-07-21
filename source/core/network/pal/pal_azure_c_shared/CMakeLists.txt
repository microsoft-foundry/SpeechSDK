#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
message(STATUS "Configuring pal_azure_c_shared")

if(use_openssl_dynamic AND use_openssl3)
    message(STATUS "Configuring dynamic pal_azure_c_shared_openssl3")
    project(pal_azure_c_shared_openssl3)
    message(STATUS "Linking with ${AZURE_SHARED_LIB_DIR_OPENSSL3}")
    link_directories(${AZURE_SHARED_LIB_DIR_OPENSSL3})  
else()
    if(NOT "${AZURE_SHARED_LIB_DIR}" STREQUAL "")
        message(STATUS "Linking with ${AZURE_SHARED_LIB_DIR}")
        link_directories(${AZURE_SHARED_LIB_DIR})
    endif()
    project(pal_azure_c_shared)
endif()

message (STATUS "Configuring ${PROJECT_NAME} ${CMAKE_CURRENT_BINARY_DIR}")
set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
)

set(HEADERS_PRIVATE
    "${SRC_DIR}/azure_c_shared_utility_http_proxy_io_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_httpapi_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_platform_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_string_token_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_tlsio_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_urlencode_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_uws_client_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_uws_frame_encoder_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_xlogging_wrapper.h"
    "${SRC_DIR}/azure_c_shared_utility_error_converter.h"
    "${SRC_DIR}/http_platform_impl.h"
    "${SRC_DIR}/http_request.h"
    "${SRC_DIR}/http_response.h"
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/web_socket.h"
    "${SRC_DIR}/i_http_adapter.h"
)

set(SOURCES
    "${SRC_DIR}/azure_c_shared_utility_error_converter.cpp"
    "${SRC_DIR}/http_platform_impl.cpp"
    "${SRC_DIR}/http_request.cpp"
    "${SRC_DIR}/http_response.cpp"
    "${SRC_DIR}/web_socket.cpp"
    "${SRC_DIR}/pal_create_module_object.cpp"
)

if(${use_winrt_networking})
    set(HEADERS_PRIVATE ${HEADERS_PRIVATE}
        "${SRC_DIR}/winrt_web_socket.h"
        "${SRC_DIR}/winrt_http_adapter.h"
    )
    set(SOURCES ${SOURCES}
       "${SRC_DIR}/winrt_web_socket.cpp"
       "${SRC_DIR}/winrt_http_adapter.cpp"
    )
else()
    set(HEADERS_PRIVATE ${HEADERS_PRIVATE}
        "${SRC_DIR}/uws_web_socket.h"
        "${SRC_DIR}/compact_http_adapter.h"
    )
    set(SOURCES ${SOURCES}
        "${SRC_DIR}/uws_web_socket.cpp"
        "${SRC_DIR}/compact_http_adapter.cpp"
    )
endif()

if(${use_openssl_dynamic})
    add_definitions(-D EXPORT_CMO)
    add_library(${PROJECT_NAME} SHARED ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})
else()
    add_library(${PROJECT_NAME} STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})
endif()

message(STATUS "Adding lib ${AZURE_SHARED_LIB}")
list(APPEND ADDITIONAL_LIBS ${AZURE_SHARED_LIB})

set_source_groups()

target_compile_definitions(${PROJECT_NAME} PUBLIC "NETWORKING_AZURE_C_SHARED")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

if(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_RPATH "$ORIGIN")
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-z,now") # https://github.com/microsoft/binskim/blob/main/docs/BinSkimRules.md#rule-BA3011EnableBindNow

    if(NOT APPLE AND use_openssl_dynamic)
        # This shared library statically links the vendored azure-c-shared-utility code
        # (via ${AZURE_SHARED_LIB}). Those objects have default ELF visibility, so their
        # internal symbols would be exported from this .so even though the only intended
        # export is Pal_CreateModuleObject.
        # To avoid a potential collision with global symbols with the same names from other
        # libraries in the same process, use --exclude-libs,ALL to force symbols brought in
        # from static archives to local binding.
        target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--exclude-libs,ALL")

        # Verify that static-archive symbols are not leaked in the dynamic export table
        # and the intended entry point remains.
        find_program(NM_EXECUTABLE nm)
        if(NM_EXECUTABLE)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DNM=${NM_EXECUTABLE}
                    -DLIBFILE=$<TARGET_FILE:${PROJECT_NAME}>
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_exports.cmake"
                VERBATIM
                COMMENT "Verifying ${PROJECT_NAME} does not leak static-archive symbols")
        else()
            message(WARNING "nm not found: skipping export-hardening verification for ${PROJECT_NAME}")
        endif()
    endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER core/network
    COMPILE_DEFINITIONS "PAL_CONFIG_EXPORTAPIS"
)

# Because of the special way we create and link iOS test projects, we need to manually specify all necessary
# static libraries to link against
list(APPEND IOS_ADDITIONAL_STATIC_LIBS
    $<TARGET_FILE:${PROJECT_NAME}>
    $<TARGET_FILE:${AZURE_SHARED_LIB}>)

if(WIN32)
    list(APPEND ADDITIONAL_LIBS rpcrt4.lib)
elseif(${use_openssl})
    list(APPEND ADDITIONAL_LIBS z dl) # libz dl
endif()

# When using WinRt, the winrt options set at the end of this file cause an issue later in the build
# when vccolib is added: vccorlib_lib_should_be_specified_before_msvcrt_lib_to_linker
# That is why, for WinRt, the vccorlib is added here eventhough it is not required by this library.
if(${use_winrt_networking})
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND ADDITIONAL_LIBS vccorlib)
    else()
        list(APPEND ADDITIONAL_LIBS vccorlibd)
    endif()
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE interfaces
    PRIVATE common
    PRIVATE util
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

if(MSVC_TOOLSET_VERSION EQUAL 141)
    set(VCPACKAGES_DIR "C:/Program Files (x86)/Microsoft Visual Studio/2017/Enterprise/Common7/IDE/VC/vcpackages")
elseif(MSVC_TOOLSET_VERSION EQUAL 142)
    set(VCPACKAGES_DIR "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/Common7/IDE/VC/vcpackages")
elseif(MSVC_TOOLSET_VERSION EQUAL 143)
    set(VCPACKAGES_DIR "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/VC/vcpackages")
endif()

if(${use_winrt_networking})
    target_compile_options(pal_azure_c_shared PRIVATE /ZW)
    target_compile_options(pal_azure_c_shared PRIVATE /EHsc)
    set( CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4264" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -AI\"${VCPACKAGES_DIR}\"" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -AI\"C:/Program Files (x86)/Windows Kits/10/UnionMetadata/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}\"" )
endif()