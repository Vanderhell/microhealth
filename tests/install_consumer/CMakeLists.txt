cmake_minimum_required(VERSION 3.21)

project(microhealth_install_consumer LANGUAGES C)

find_package(microhealth CONFIG REQUIRED)

add_executable(microhealth_install_consumer main.c)
target_link_libraries(microhealth_install_consumer PRIVATE microhealth::microhealth)
set_target_properties(microhealth_install_consumer PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
