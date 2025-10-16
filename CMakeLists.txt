cmake_minimum_required(VERSION 3.15)

project(wujihandcpp LANGUAGES C CXX)

# Get the latest Git tag
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_TAG_RESULT
)

# Extract version number if tag format is valid
if(GIT_TAG_RESULT EQUAL 0 AND GIT_TAG MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
    string(REGEX REPLACE "^v" "" PROJECT_VERSION ${GIT_TAG})
    message(STATUS "Using Git tag version: ${PROJECT_VERSION}")
else()
    set(PROJECT_VERSION 0.0.0)
    message(STATUS "Unable to extract git tag, using version: ${PROJECT_VERSION}")
endif()

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Use FetchContent to get spdlog
include(FetchContent)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
)

# Configure spdlog options before making it available
set(SPDLOG_BUILD_PIC ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# Set C standard to C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Disable GNU extensions
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add compiler options based on compiler
if(MSVC)
    add_compile_options(/W4 /Zc:preprocessor)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    # GCC/Clang
    add_compile_options(-Wall -Wextra -Wpedantic -fvisibility=hidden)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(STATIC_LINK_STDCPP "Statically link the C++ standard library" OFF)
if(MSVC)
    if(STATIC_LINK_STDCPP)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        message(STATUS "Static linking C++ standard library (MSVC)")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        message(STATUS "Dynamic linking C++ standard library (MSVC)")
    endif()
endif()

# Get project sources
file(GLOB_RECURSE PROJECT_SOURCE CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/src/*.c
)

option(BUILD_STATIC_WUJIHANDCPP "Build static library wujihandcpp" OFF)
if(BUILD_STATIC_WUJIHANDCPP)
    add_library(
        ${PROJECT_NAME} STATIC
        ${PROJECT_SOURCE}
    )
    target_compile_definitions(${PROJECT_NAME} PUBLIC STATIC_LINKING_WUJIHANDCPP)
    message(STATUS "Building static library wujihandcpp")
else()
    add_library(
        ${PROJECT_NAME} SHARED
        ${PROJECT_SOURCE}
    )
    message(STATUS "Building shared library wujihandcpp")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE BUILDING_WUJIHANDCPP)

include_directories(SYSTEM ${LIBUSB_INCLUDE_DIRS})

target_include_directories(${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

if(WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${LIBUSB_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PUBLIC "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBUSB_LIBRARIES})
elseif(UNIX)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE /usr/include/libusb-1.0)
    target_link_libraries(${PROJECT_NAME} PRIVATE usb-1.0)
endif()

# Link spdlog library
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)

# Enable LTO optimization
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION TRUE
)

option(STATIC_LINK_STDCPP "Statically link the C++ standard library" OFF)
if(STATIC_LINK_STDCPP)
    if(MSVC)
        message(FATAL_ERROR "Not currently supported.")
    else()
        # GCC/Clang
if(NOT MSVC)
    # GCC/Clang
    if(STATIC_LINK_STDCPP)
        target_compile_options(${PROJECT_NAME} PRIVATE
            -fdata-sections
            -ffunction-sections
        )
        target_link_options(${PROJECT_NAME} PRIVATE -static-libstdc++ -static-libgcc)
        target_link_options(${PROJECT_NAME} PRIVATE -Wl,--exclude-libs,ALL -Wl,--gc-sections)
        message(STATUS "Static linking C++ standard library")
    else()
        message(STATUS "Dynamic linking C++ standard library")
    endif()
endif()

# Enable LTO optimization
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION TRUE
)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")

    set(ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
    if(ARCHITECTURE STREQUAL "x86_64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
    elseif(ARCHITECTURE STREQUAL "aarch64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "aarch64")
    else()
        message(FATAL_ERROR "Unsupported architecture: ${ARCHITECTURE}.")
    endif()

    set(CPACK_DEBIAN_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Wuji Technology <support@pan-motor.com>")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "A Lightweight C++ SDK for Wujihand")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Wuji-Technology-Co-Ltd/wujihandcpp")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libusb-1.0-0-dev")
    set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

    set(CPACK_RPM_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_RPM_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_RPM_PACKAGE_VENDOR "Wuji Technology <support@pan-motor.com>")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "A Lightweight C++ SDK for Wujihand")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/Wuji-Technology-Co-Ltd/wujihandcpp")
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_REQUIRES "libusbx-devel")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")

    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

    include(CPack)
endif()

# Option to enable tests
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    # Enable CTest
    include(CTest)
    # Use FetchContent to get GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Add test directory for all test files
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/test/*.cpp
    )

    foreach(test_source ${TEST_SOURCES})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE wujihandcpp GTest::gtest_main)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    message(STATUS "Tests enabled")
else()
    message(STATUS "Tests disabled")
endif()
