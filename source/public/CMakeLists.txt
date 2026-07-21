#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
add_subdirectory(c_api)
add_subdirectory(cxx_api)

if (WIN32)
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.26.3")  
        add_custom_target(${PROJECT_NAME}_HEADERS ALL
            COMMENT "Copying core headers from ${CMAKE_CURRENT_SOURCE_DIR} to ${CMAKE_CURRENT_BINARY_DIR}"

            # Speech API's
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/cxx_api
                ${CMAKE_CURRENT_BINARY_DIR}/apiview/cxx_api/inc
            # No API View for now....

            # Copy the C headers to keep the reference paths sane.
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/c_api
                ${CMAKE_CURRENT_BINARY_DIR}/apiview/c_api
        )

        set_target_properties(${PROJECT_NAME}_HEADERS PROPERTIES FOLDER "api")
     else()  
        message(STATUS "Skipping API View C++ header copy becasue CMake version is less than 3.26.3")  
    endif()  
endif()

