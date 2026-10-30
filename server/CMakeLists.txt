cmake_minimum_required(VERSION 3.15)

project(LeDashboard_Server LANGUAGES CXX)

# Version
set(PROJECT_VERSION "1.0.0")

# Configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project description
set(PROJECT_DESCRIPTION "LeDashboard Server Application - C++ Host Software")

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# Output directory (must be set before add_executable so targets pick it up)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Executable
add_executable(ledashboard_server ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(ledashboard_server PRIVATE include)

# Options
option(USE_JSON_FORMAT "Use JSON format for communication" ON)
option(USE_LZ4_COMPRESSION "Use LZ4 compression" ON)

if(USE_JSON_FORMAT)
    target_compile_definitions(ledashboard_server PRIVATE USE_JSON_FORMAT=1)
else()
    target_compile_definitions(ledashboard_server PRIVATE USE_BINARY_FORMAT=1)
endif()

if(USE_LZ4_COMPRESSION)
    target_compile_definitions(ledashboard_server PRIVATE USE_LZ4=1)
endif()

# Dependencies (vcpkg manifest mode - see vcpkg.json; toolchain wired in CMakePresets.json)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(ledashboard_server PRIVATE PLATFORM_WINDOWS=1)
    # Windows-specific libraries
    target_link_libraries(ledashboard_server PRIVATE Setupapi)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(ledashboard_server PRIVATE PLATFORM_LINUX=1)
elseif(APPLE)
    target_compile_definitions(ledashboard_server PRIVATE PLATFORM_MACOS=1)
endif()

# Link dependencies
target_link_libraries(ledashboard_server PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    cpr::cpr
    lz4::lz4
)

# Installation
install(TARGETS ledashboard_server
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Print configuration
message(STATUS "LeDashboard Server Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  JSON Format: ${USE_JSON_FORMAT}")
message(STATUS "  LZ4 Compression: ${USE_LZ4_COMPRESSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
