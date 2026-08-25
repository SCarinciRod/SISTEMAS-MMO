cmake_minimum_required(VERSION 3.20)

project(SISTEMAS_MMO VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(MMO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(MMO_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MMO_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(MMO_ENABLE_TSAN "Enable ThreadSanitizer (future diagnostic use)" OFF)
option(MMO_ENABLE_LUA "Enable the Lua content loader" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MMO_ENABLE_TSAN AND MMO_ENABLE_ASAN)
    message(FATAL_ERROR "MMO_ENABLE_TSAN and MMO_ENABLE_ASAN cannot be enabled together")
endif()

add_library(mmo_build_options INTERFACE)

if(MSVC)
    target_compile_options(mmo_build_options INTERFACE /W4 /EHsc /permissive- /Zc:__cplusplus)

    if(MMO_WARNINGS_AS_ERRORS)
        target_compile_options(mmo_build_options INTERFACE /WX)
    endif()

    if(MMO_ENABLE_ASAN)
        target_compile_options(mmo_build_options INTERFACE /fsanitize=address)
        target_link_options(mmo_build_options INTERFACE /fsanitize=address)
    endif()

    if(MMO_ENABLE_UBSAN)
        message(WARNING "UBSan is not supported by this MSVC configuration; option ignored")
    endif()

    if(MMO_ENABLE_TSAN)
        message(WARNING "TSan is not supported by this MSVC configuration; option ignored")
    endif()
else()
    target_compile_options(mmo_build_options INTERFACE -Wall -Wextra -Wpedantic)

    if(MMO_WARNINGS_AS_ERRORS)
        target_compile_options(mmo_build_options INTERFACE -Werror)
    endif()

    if(MMO_ENABLE_ASAN)
        target_compile_options(mmo_build_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(mmo_build_options INTERFACE -fsanitize=address)
    endif()

    if(MMO_ENABLE_UBSAN)
        target_compile_options(mmo_build_options INTERFACE -fsanitize=undefined -fno-omit-frame-pointer)
        target_link_options(mmo_build_options INTERFACE -fsanitize=undefined)
    endif()

    if(MMO_ENABLE_TSAN)
        target_compile_options(mmo_build_options INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(mmo_build_options INTERFACE -fsanitize=thread)
    endif()
endif()

add_library(mmo_core INTERFACE)
target_include_directories(mmo_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_core INTERFACE mmo_build_options)

add_library(mmo_persistence STATIC src/mmo/persistence/lua_loader.cpp)
target_link_libraries(mmo_persistence PUBLIC mmo_core)

if(MMO_ENABLE_LUA)
    find_package(Lua REQUIRED)
    target_compile_definitions(mmo_persistence PUBLIC MMO_USE_LUA)
    target_include_directories(mmo_persistence PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(mmo_persistence PUBLIC ${LUA_LIBRARIES})
endif()

add_executable(mmo_server src/main.cpp)
target_link_libraries(mmo_server PRIVATE mmo_core mmo_persistence)

if(BUILD_TESTING)
    add_executable(mmo_unit_tests src/mmo/tests/unit/event_tests.cpp)
    target_link_libraries(mmo_unit_tests PRIVATE mmo_core)

    add_test(NAME mmo_unit_tests COMMAND mmo_unit_tests)
    set_tests_properties(mmo_unit_tests PROPERTIES LABELS "unit")

    add_executable(
        mmo_simulation_tests
        src/mmo/tests/integration/simulation_tests.cpp)
    target_link_libraries(mmo_simulation_tests PRIVATE mmo_core)

    add_test(NAME mmo_simulation_tests COMMAND mmo_simulation_tests)
    set_tests_properties(
        mmo_simulation_tests
        PROPERTIES LABELS "integration;simulation")

    if(MMO_ENABLE_LUA)
        add_executable(
            mmo_lua_integration_tests
            src/mmo/tests/integration/lua_loader_tests.cpp)
        target_link_libraries(mmo_lua_integration_tests PRIVATE mmo_persistence)
        target_compile_definitions(
            mmo_lua_integration_tests
            PRIVATE
                MMO_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
                MMO_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")

        add_test(NAME mmo_lua_integration_tests COMMAND mmo_lua_integration_tests)
        set_tests_properties(
            mmo_lua_integration_tests
            PROPERTIES LABELS "integration;lua")
    else()
        add_executable(
            mmo_lua_disabled_tests
            src/mmo/tests/integration/lua_loader_disabled_tests.cpp)
        target_link_libraries(mmo_lua_disabled_tests PRIVATE mmo_persistence)

        add_test(NAME mmo_lua_disabled_tests COMMAND mmo_lua_disabled_tests)
        set_tests_properties(
            mmo_lua_disabled_tests
            PROPERTIES LABELS "integration;lua-disabled")
    endif()

    add_executable(mmo_stress_tests src/mmo/tests/stress_tests.cpp)
    target_link_libraries(mmo_stress_tests PRIVATE mmo_core mmo_persistence)

    add_test(NAME mmo_stress_tests COMMAND mmo_stress_tests)
    set_tests_properties(mmo_stress_tests PROPERTIES
        ENVIRONMENT "MMO_STRESS_LEVEL=smoke"
        LABELS "stress"
    )
endif()
