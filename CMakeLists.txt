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
set(CMAKE_PROJECT_NAME wolftrack-node)

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

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
# User C++ and board-selection configuration.
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BOARD "FRONT" CACHE STRING "Sensor board variant (FRONT or REAR)")
set_property(CACHE BOARD PROPERTY STRINGS FRONT REAR)

if(BOARD STREQUAL "FRONT")
    set(BOARD_SOURCE Board/board_front.cpp)
    set(BOARD_DEFINE BOARD_FRONT=1)
elseif(BOARD STREQUAL "REAR")
    set(BOARD_SOURCE Board/board_rear.cpp)
    set(BOARD_DEFINE BOARD_REAR=1)
else()
    message(FATAL_ERROR "BOARD must be FRONT or REAR; got '${BOARD}'")
endif()
string(TOLOWER "${BOARD}" BOARD_NAME)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    App/Src/app_logic.cpp
    App/Src/dfu_trigger.c
    App/Src/usb_identity.c
    ${BOARD_SOURCE}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    App/Inc
    Board
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    ${BOARD_DEFINE}
    APP_BOARD_NAME=\"${BOARD_NAME}\"
    APP_BUILD_TYPE=\"${CMAKE_BUILD_TYPE}\"
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Emit a raw flash image alongside the ELF after every successful link.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.bin
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin"
)
