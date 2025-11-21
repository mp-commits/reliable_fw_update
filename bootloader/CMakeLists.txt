cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME bootloader)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

add_subdirectory(../FwUpdateLibs ${CMAKE_CURRENT_BINARY_DIR}/FwUpdateLibs)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/app_status.c
    Core/Src/installer.c
    Core/Src/w25qxx_init.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_BINARY_DIR}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    libs::niram
    libs::crc
    libs::w25qxx
    libs::fragmentstore
    libs::ed25519
)

add_custom_target(generated_key_file
    COMMAND generate_keyfile -i $ENV{FW_SIGNING_KEY} -o ${CMAKE_BINARY_DIR}/generated_public_key.h
)
add_dependencies(${PROJECT_NAME} generated_key_file)

add_custom_target(Flash_to_STM32
  COMMAND ${CMAKE_COMMAND} -E echo "Flashing board..."
  COMMAND STM32_Programmer_CLI -c port=SWD -d $<TARGET_FILE:${CMAKE_PROJECT_NAME}> -v -rst
  DEPENDS $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
)
