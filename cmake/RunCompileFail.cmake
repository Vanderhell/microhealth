if(NOT DEFINED TEST_SOURCE OR NOT DEFINED INCLUDE_DIR OR NOT DEFINED C_COMPILER)
    message(FATAL_ERROR "TEST_SOURCE, INCLUDE_DIR, and C_COMPILER are required")
endif()

set(object_file "${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.obj")
if(C_COMPILER_ID STREQUAL "MSVC")
    execute_process(
        COMMAND "${C_COMPILER}" /nologo /c /TC /WX /I "${INCLUDE_DIR}" "${TEST_SOURCE}" /Fo"${object_file}"
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
else()
    execute_process(
        COMMAND "${C_COMPILER}" -std=c99 -Wall -Wextra -Wpedantic -Werror -I "${INCLUDE_DIR}" -c "${TEST_SOURCE}" -o "${object_file}"
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
endif()

if(rc EQUAL 0)
    message(FATAL_ERROR "compile-fail test ${TEST_NAME} unexpectedly compiled")
endif()
