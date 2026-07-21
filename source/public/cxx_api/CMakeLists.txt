#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
cmake_minimum_required(VERSION 3.19)

project(cxx_headers)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
add_library(${PROJECT_NAME} INTERFACE ${SPEECH_CXX_API_HEADERS})
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} INTERFACE c_headers)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER api)
