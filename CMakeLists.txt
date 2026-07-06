cmake_minimum_required(VERSION 3.20)
# Set CRT linkage for vcpkg dependencies to static to avoid runtime DLL dependencies on Windows
set(VCPKG_CRT_LINKAGE static)
# Define CMAKE C++ standards
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Ensures portable code
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(NPET_comm_FW LANGUAGES C CXX)

enable_testing()

# Find dependencies
find_package(CLI11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_path(RANG_INCLUDE_DIRS "rang.hpp") # Rang is header-only, so we just need to find the include directory
# HAS to use MSYS2 Python!!! READ README.md for installation instructions
if (DEFINED ENV{MINGW_PREFIX})
    set(Python3_ROOT_DIR "$ENV{MINGW_PREFIX}")
endif ()
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
# Improve pybind11's ability to find the correct Python installation, especially on Windows
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)
find_package(Git REQUIRED)

# Generate the header from MANUAL.md
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/manual_data.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding MANUAL.md into header..."
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/embed_text_file.py
        --source ${CMAKE_CURRENT_SOURCE_DIR}/MANUAL.md
        --destination ${CMAKE_CURRENT_BINARY_DIR}/generated/manual_data.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/MANUAL.md
        ${CMAKE_CURRENT_SOURCE_DIR}/embed_text_file.py
        COMMENT "Generating manual_data.h from MANUAL.md"
        VERBATIM
)
add_custom_target(manual_header
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated/manual_data.h
)

# Generate the header from Licenses
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/license_data.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding LICENSES into header..."
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/embed_text_file.py
        --source ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        --destination ${CMAKE_CURRENT_BINARY_DIR}/generated/license_data.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/embed_text_file.py
        --source ${CMAKE_CURRENT_SOURCE_DIR}/NOTICE
        --destination ${CMAKE_CURRENT_BINARY_DIR}/generated/license_data.h
        -ad
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/embed_text_file.py
        --source ${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt
        --destination ${CMAKE_CURRENT_BINARY_DIR}/generated/license_data.h
        -ad
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/NOTICE
        ${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/embed_text_file.py
        COMMENT "Generating license_data.h from LICENSES"
        VERBATIM
)
add_custom_target(license_header
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated/license_data.h
)

# Create a script that generates the header with Git tag information
add_custom_target(git_tag_header
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding GIT_TAG into header..."
        COMMAND ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/generated/git_tag.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_git_tag.cmake
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/generated/git_tag.h
        COMMENT "Generating program version from Git tag..."
        VERBATIM
)

# List of framework source files
file(GLOB_RECURSE NPET_FRAMEWORK_FILES "src/framework/*.cpp")
# List of CLI source files
file(GLOB_RECURSE NPET_CLI_FILES "src/cli/*.cpp")


# Define the core library that contains the main framework logic, which can be used by both the CLI and the Python bindings
function(configure_framework_target TARGET_NAME)
    target_include_directories(${TARGET_NAME} PUBLIC
            ${CMAKE_CURRENT_BINARY_DIR}/generated
            ${CMAKE_CURRENT_SOURCE_DIR}/src/framework
    )
    target_link_libraries(${TARGET_NAME} PUBLIC Boost::system spdlog::spdlog)
    target_link_libraries(${TARGET_NAME} PRIVATE setupapi quadmath ws2_32 mswsock)
endfunction()
# CLI variant — debug logging
add_library(NPET_comm_FW_framework ${NPET_FRAMEWORK_FILES})
configure_framework_target(NPET_comm_FW_framework)
target_compile_definitions(NPET_comm_FW_framework PRIVATE
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
)
# Test variant — silent
add_library(NPET_comm_FW_framework_silent ${NPET_FRAMEWORK_FILES})
configure_framework_target(NPET_comm_FW_framework_silent)
target_compile_definitions(NPET_comm_FW_framework_silent PRIVATE
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF
)


# Compile source code into python package
pybind11_add_module(NPET_comm_FW_lib
        src/bindings_to_python.cpp
)
target_link_libraries(NPET_comm_FW_lib PRIVATE
        NPET_comm_FW_framework
        pybind11::pybind11
        Python3::Python
)
add_dependencies(NPET_comm_FW_lib manual_header git_tag_header license_header)
target_compile_definitions(NPET_comm_FW_lib PRIVATE PYBIND11_ENABLED)


# Compile source code into CLI executable
add_executable(NPET_comm_FW_CLI
        ${NPET_CLI_FILES}
        src/main.cpp
        src/virtual_machine/virtual_machine.cpp
)
target_link_libraries(NPET_comm_FW_CLI PRIVATE
        NPET_comm_FW_framework
        CLI11::CLI11
)
target_include_directories(NPET_comm_FW_CLI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/src/virtual_machine
)
add_dependencies(NPET_comm_FW_CLI manual_header git_tag_header license_header)
target_compile_definitions(NPET_comm_FW_CLI PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
# Add exe icon on Windows
if (WIN32)
    target_sources(NPET_comm_FW_CLI PRIVATE src/icon/app.rc)
endif ()
# On Windows with MinGW, statically link GCC and C++ standard libraries to avoid runtime DLL dependencies
# This makes sure the program builds into a standalone executable
if (WIN32 AND MINGW)
    target_link_options(NPET_comm_FW_CLI PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
            # Fix for multiple definition of std::type_info with static linking.
            # known issue with statically linked MINGW
            -Wl,--allow-multiple-definition
    )
endif ()


# Define tests for the framework using Google Test
add_subdirectory(tests)
