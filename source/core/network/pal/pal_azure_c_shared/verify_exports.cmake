# Post-build verification for the pal_azure_c_shared[_openssl3] shared library.
#
# Confirms that the --exclude-libs,ALL hardening took effect:
#   1. The vendored azure-c-shared-utility helper get_time() must NOT be a
#      globally-exported (dynamic, defined) symbol.
#   2. The intended plugin entry point Pal_CreateModuleObject MUST still be
#      exported.
#
# Invoked via: cmake -DNM=<nm> -DLIBFILE=<path> -P verify_exports.cmake
#
# nm -D prints defined dynamic symbols with an uppercase type code (T/W for text,
# D/B for data); undefined symbols use 'U' and local/hidden symbols use lowercase.
# We therefore only fail on an uppercase (exported) get_time.

if(NOT DEFINED NM OR NOT DEFINED LIBFILE)
    message(FATAL_ERROR "verify_exports.cmake requires -DNM=<nm> and -DLIBFILE=<lib>")
endif()

execute_process(
    COMMAND ${NM} -D --defined-only ${LIBFILE}
    OUTPUT_VARIABLE _exports
    RESULT_VARIABLE _nm_rc
    ERROR_VARIABLE _nm_err)

if(NOT _nm_rc EQUAL 0)
    message(FATAL_ERROR "verify_exports: '${NM} -D --defined-only ${LIBFILE}' failed (rc=${_nm_rc}): ${_nm_err}")
endif()

# (1) get_time must not be an exported defined symbol.
#     Match a line ending in a defined-symbol type code followed by 'get_time'.
if(_exports MATCHES "[ \t][TWtDB][ \t]get_time([ \t]|\n|$)")
    message(FATAL_ERROR
        "verify_exports: FAIL -- '${LIBFILE}' still exports get_time as a defined dynamic symbol.")
endif()

# (2) The intended entry point must remain exported.
if(NOT _exports MATCHES "Pal_CreateModuleObject")
    message(FATAL_ERROR
        "verify_exports: FAIL -- '${LIBFILE}' no longer exports Pal_CreateModuleObject.")
endif()
