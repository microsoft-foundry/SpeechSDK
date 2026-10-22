# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.12)

compileAsC99()
set(theseTestsName tlsio_openssl_ut)

set(${theseTestsName}_test_files
    ${theseTestsName}.c
    ../tlsio_test_server/tls_test_server.c
)

include_directories(../tlsio_test_server)

build_c_test_artifacts(${theseTestsName} OFF "tests/azure_c_shared_utility_tests" ADDITIONAL_LIBS aziotsharedutil)
