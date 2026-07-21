#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(network_pal)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(SOURCES
)

set(HEADERS
    "${INC_DIR}/pal_create_module_object.h"
)

set_source_groups()

add_library(${PROJECT_NAME} INTERFACE ${HEADERS})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${INC_DIR}
)

add_subdirectory(pal_binding)
target_link_libraries(${PROJECT_NAME} INTERFACE pal_binding)

# individual components
if (NATIVE_NETWORKING_IMPL STREQUAL "Azure_C_Shared")
  # Always build / link an OpenSSL library
  message (STATUS "Adding Azure C Shared")
  
  if(${use_openssl_dynamic})
    message (STATUS "Adding Dynamic Azure C Shared library")
    # If we're building a dynamic openssl that we'll need to pick betwen 1.1.1 and 3.0 on the fly,
    # add the dynamic picker library and link it in.
    add_subdirectory(pal_dynamic_azure_c_shared)
    target_link_libraries(${PROJECT_NAME} INTERFACE pal_dynamic_azure_c_shared)

    if(NOT BUILD_CORE_ONLY)
      # Add the Azure C Shared library that uses the default OpenSSL.
      message(STATUS "Adding Azure C Shared library that uses the default OpenSSL")
      list(APPEND ADDITIONAL_LIBS ${SPEECHSDK_CORE_LIBRARY} )
      add_subdirectory(pal_azure_c_shared)
    endif()

    # And then add the Azure C Shared library again, with 1.1.1 support.
    message (STATUS "Adding OpenSSL 3 explicitly") 
    set(use_openssl3 ON)
    add_subdirectory(pal_azure_c_shared pal_azure_c_shared_openssl3)
  else()
    message (STATUS "Adding Azure C Shared library as a static library")
    add_subdirectory(pal_azure_c_shared)
    # If we're not doing dynamic openssl, just link the Azure C Shared library that uses the default OpenSSL.
    target_link_libraries(${PROJECT_NAME} INTERFACE pal_azure_c_shared)
  endif()

elseif(NATIVE_NETWORKING_IMPL STREQUAL "Azure_Core")
  add_subdirectory(pal_azure_core)
  target_link_libraries(${PROJECT_NAME} INTERFACE pal_azure_core)
elseif(NATIVE_NETWORKING_IMPL STREQUAL "None")
  add_subdirectory(pal_no_networking)
  target_link_libraries(${PROJECT_NAME} INTERFACE pal_no_networking)
else()
  message(FATAL_ERROR "Unsupported native networking implementation: '$CACHE{NATIVE_NETWORKING_IMPL}'")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER core/network)
