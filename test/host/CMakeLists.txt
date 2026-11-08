# Host tests for the IDF-free core in main/core. Plain CMake and Unity;
# no ESP-IDF needed.
#
#   cmake -S test/host -B build-host && cmake --build build-host
#   ctest --test-dir build-host --output-on-failure
cmake_minimum_required(VERSION 3.22)
project(gravity_can_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(unity)

set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../main/core)
file(GLOB CORE_SRCS CONFIGURE_DEPENDS ${CORE_DIR}/*.c)

add_compile_options(-Wall -Wextra -Werror)

enable_testing()
file(GLOB TEST_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/test_*.c)
foreach(src ${TEST_SRCS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src} ${CORE_SRCS})
    target_include_directories(${name} PRIVATE ${CORE_DIR})
    target_link_libraries(${name} PRIVATE unity m)
    add_test(NAME ${name} COMMAND ${name})
endforeach()
