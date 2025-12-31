cmake_minimum_required(VERSION 3.15)
project(UEFIApp VERSION 0.1.0 LANGUAGES C ASM)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set MinGW-w64 as the compiler
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_ASM_COMPILER x86_64-w64-mingw32-gcc)

# Prevent CMake from adding standard libraries
set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_C_STANDARD_LIBRARIES_INIT "")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/inc/efi
    ${CMAKE_SOURCE_DIR}/inc/efi/x86_64
    ${CMAKE_SOURCE_DIR}/inc/efi/protocol
)

# Compiler flags for UEFI
set(UEFI_C_FLAGS
    -nostdlib
    -ffreestanding
    -fno-stack-protector
    -fshort-wchar
    -mno-red-zone
    -Wall
    -Wextra
)

# Linker flags for UEFI application
set(UEFI_LINK_FLAGS
    -nostdlib
    -Wl,--subsystem,10
    -Wl,--entry=efi_main
    -Wl,-dll
    -Wl,--no-seh
)

# Collect all source files
file(GLOB_RECURSE SOURCES 
    ${CMAKE_SOURCE_DIR}/src/*.c
)

# Create the UEFI application
add_executable(UEFIApp ${SOURCES})

# Apply flags
target_compile_options(UEFIApp PRIVATE ${UEFI_C_FLAGS})
target_link_options(UEFIApp PRIVATE ${UEFI_LINK_FLAGS})

# Set output properties
set_target_properties(UEFIApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    SUFFIX ".efi"
)

# Print configuration info
message(STATUS "UEFI Application Configuration:")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Source directory: ${CMAKE_SOURCE_DIR}/src")
message(STATUS "  Include directory: ${CMAKE_SOURCE_DIR}/inc")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/UEFIApp.efi")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Compile commands: ${CMAKE_BINARY_DIR}/compile_commands.json")
