cmake_minimum_required(VERSION 3.21)

project(microhealth LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    set(MICROHEALTH_WARNING_C_FLAGS /W4 /WX)
else()
    set(MICROHEALTH_WARNING_C_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wcast-qual
        -Wwrite-strings
        -Wformat=2
        -Werror
    )
endif()

function(microhealth_apply_project_warnings target)
    target_compile_options(${target} PRIVATE ${MICROHEALTH_WARNING_C_FLAGS})
endfunction()

add_library(microhealth STATIC src/mhealth.c)
add_library(microhealth::microhealth ALIAS microhealth)
target_include_directories(
    microhealth
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(microhealth PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
microhealth_apply_project_warnings(microhealth)

add_executable(microhealth_runtime_tests tests/test_all.c)
target_link_libraries(microhealth_runtime_tests PRIVATE microhealth)
set_target_properties(microhealth_runtime_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
microhealth_apply_project_warnings(microhealth_runtime_tests)
add_test(NAME runtime_tests_c99 COMMAND microhealth_runtime_tests)

add_executable(microhealth_runtime_tests_c11 tests/test_all.c)
target_link_libraries(microhealth_runtime_tests_c11 PRIVATE microhealth)
set_target_properties(microhealth_runtime_tests_c11 PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
microhealth_apply_project_warnings(microhealth_runtime_tests_c11)
add_test(NAME runtime_tests_c11 COMMAND microhealth_runtime_tests_c11)

add_executable(microhealth_include_twice_c tests/include_twice.c)
target_link_libraries(microhealth_include_twice_c PRIVATE microhealth)
set_target_properties(microhealth_include_twice_c PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
microhealth_apply_project_warnings(microhealth_include_twice_c)

add_executable(microhealth_multi_tu_c tests/multi_tu_a.c tests/multi_tu_b.c tests/multi_tu_main.c)
target_link_libraries(microhealth_multi_tu_c PRIVATE microhealth)
set_target_properties(microhealth_multi_tu_c PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
microhealth_apply_project_warnings(microhealth_multi_tu_c)

add_executable(microhealth_cpp_consumer tests/consumer_cpp.cpp)
target_link_libraries(microhealth_cpp_consumer PRIVATE microhealth)
target_include_directories(microhealth_cpp_consumer PRIVATE include)
set_target_properties(microhealth_cpp_consumer PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(microhealth_cpp_consumer PRIVATE /W4 /WX)
else()
    target_compile_options(microhealth_cpp_consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME cpp_consumer COMMAND microhealth_cpp_consumer)

add_executable(microhealth_readme_example tests/readme_example.c)
target_link_libraries(microhealth_readme_example PRIVATE microhealth)
set_target_properties(microhealth_readme_example PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
microhealth_apply_project_warnings(microhealth_readme_example)

set(COMPILE_FAIL_SOURCES
    wrong_clock_signature
    wrong_collect_signature
    wrong_alert_signature
    old_layout_assumption
)

foreach(name IN LISTS COMPILE_FAIL_SOURCES)
    add_test(
        NAME compile_fail_${name}
        COMMAND
            ${CMAKE_COMMAND}
            -DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail/${name}.c
            -DTEST_NAME=${name}
            -DINCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/include
            -DC_COMPILER=${CMAKE_C_COMPILER}
            -DC_COMPILER_ID=${CMAKE_C_COMPILER_ID}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunCompileFail.cmake
    )
endforeach()

install(TARGETS microhealth EXPORT microhealthTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES include/mhealth.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    cmake/microhealthConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/microhealthConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/microhealth
)

install(
    EXPORT microhealthTargets
    NAMESPACE microhealth::
    FILE microhealthTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/microhealth
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/microhealthConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/microhealth
)
