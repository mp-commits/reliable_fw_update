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
set(CMAKE_PROJECT_NAME new_freertos_app)

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

add_subdirectory($ENV{FW_UPDATE_LIBS_DIR} ${CMAKE_CURRENT_BINARY_DIR}/FwUpdateLibs)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/bigendian.c
    Core/Src/driver_w25qxx.c
    Core/Src/metadata.c
    Core/Src/no_init_ram.c
    Core/Src/tcpserver.c
    Core/Src/udpserver.c
    Core/Src/system_reset.c
    Core/Src/w25qxx_init.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    libs::fragmentstore
    libs::updateserver
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O ihex --gap-fill=0x00
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.hex
    DEPENDS $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Converting ELF to HEX with gap fill"
)

add_custom_target(
    create_hex ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}_signed.hex
    COMMAND hexsign.exe
            -i ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
            -o ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}_signed.hex
            -k $ENV{FW_SIGNING_KEY}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMENT "Signing built HEX file"
)

add_custom_target(
    signed_hex_file ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}_signed.hex
)

add_custom_target(Flash_to_STM32
  COMMAND ${CMAKE_COMMAND} -E echo "Flashing board..."
  COMMAND updateclient.exe -a 192.168.1.50 -p 7 -i ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}_signed.hex
  DEPENDS signed_hex_file
)
