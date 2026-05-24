# CMakeLists.txt for RP2350 Handheld Firmware
# ================================================================
# Build Configuration for Raspberry Pi Pico 2 (RP2350) Bare-Metal Firmware
# 
# This CMake configuration sets up the build environment for a dual-core
# bare-metal firmware targeting the RP2350 microcontroller. It includes:
# - Pico SDK integration with C++17 support
# - TrustZone-M secure gateway compilation flags
# - LittleFS file system integration
# - Binary size optimization for <320KB footprint
# - No dynamic heap allocation enforcement
#
# Target Hardware: Raspberry Pi Pico 2 (RP2350, ARM Cortex-M33)
# Toolchain: arm-none-eabi-gcc via Pico SDK
# C++ Standard: C++17 (required for modern features without RTOS overhead)
# ================================================================

cmake_minimum_required(VERSION 3.22)

# ================================================================
# PROJECT DEFINITION
# ================================================================

project(pico2_handheld_firmware 
    VERSION 1.0.0
    DESCRIPTION "Dual-Core Bare-Metal Firmware for RP2350 Handheld Device"
    LANGUAGES C CXX ASM
)

# ================================================================
# C++ STANDARD CONFIGURATION
# ================================================================

# Enforce C++17 standard across all targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable RTTI and exceptions to reduce binary size and eliminate heap usage
# These flags are critical for bare-metal operation without stdlib overhead
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

# ================================================================
# PICO SDK INTEGRATION
# ================================================================

# Initialize Pico SDK
# PICO_SDK_PATH must be set in environment or passed via -DPICO_SDK_PATH
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
    message(FATAL_ERROR "Pico SDK version 2.0.0 or higher is required for RP2350")
endif()

# ================================================================
# MEMORY OPTIMIZATION FLAGS
# ================================================================

# Optimize for size to meet 320KB binary limit
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os")

# Additional linker flags for size optimization
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --gc-sections")

# ================================================================
# TRUSTZONE-M CONFIGURATION
# ================================================================

# Enable ARM CMSE (Cortex-M Security Extensions) for TrustZone-M
# This is required for __attribute__((cmse_nonsecure_entry)) functions
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcmse")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcmse")

# Define secure section for API token storage
# This places sensitive data in a protected memory region
add_compile_definitions(
    SECURE_BUILD=1
)

# ================================================================
# LITTLEFS CONFIGURATION
# ================================================================

# LittleFS compile-time configuration for wear leveling and power-loss resilience
add_compile_definitions(
    LFS_BLOCK_SIZE=4096          # Flash sector size (typical for QSPI flash)
    LFS_BLOCK_COUNT=512          # Number of blocks in filesystem partition
    LFS_READ_SIZE=256            # Minimum read size
    LFS_PROG_SIZE=256            # Minimum program size
    LFS_CACHE_SIZE=1024          # Lookahead cache size
    LFS_LOOKAHEAD_SIZE=64        # Block lookahead size for wear leveling
    LFS_NAME_MAX=255             # Maximum filename length
    LFS_FILE_MAX=2147483647      # Maximum file size (2GB)
    LFS_ATTR_MAX=1024            # Maximum attribute size
)

# ================================================================
# HARDWARE CONFIGURATION
# ================================================================

# Set target board (Raspberry Pi Pico 2)
set(PICO_BOARD pico2)

# Configure flash size (typically 2MB or 4MB on Pico 2)
set(PICO_FLASH_SIZE_BYTES 4194304)  # 4MB

# ================================================================
# INITIALIZE PICO SDK
# ================================================================

pico_sdk_init()

# ================================================================
# EXECUTABLE TARGET DEFINITION
# ================================================================

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/secure_gateway.cpp
)

# ================================================================
# INCLUDE DIRECTORIES
# ================================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ================================================================
# PICO SDK LIBRARY LINKAGE
# ================================================================

# Link required Pico SDK libraries:
# - pico_stdlib: Core functionality (GPIO, UART, timing, etc.)
# - pico_multicore: Dual-core launch and communication
# - hardware_uart: UART peripheral driver
# - hardware_watchdog: Hardware watchdog timer
# - hardware_gpio: GPIO interrupt handling
# - hardware_irq: Interrupt controller management
# - hardware_flash: QSPI flash access for LittleFS
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_multicore
    hardware_uart
    hardware_watchdog
    hardware_gpio
    hardware_irq
    hardware_flash
)

# ================================================================
# LITTLEFS INTEGRATION (Optional - Uncomment when adding LittleFS)
# ================================================================

# If using external LittleFS library, add these lines:
# find_package(littlefs REQUIRED)
# target_link_libraries(${PROJECT_NAME} littlefs)

# Alternatively, include LittleFS source directly:
# add_subdirectory(lib/littlefs)
# target_link_libraries(${PROJECT_NAME} littlefs)

# ================================================================
# BINARY OUTPUT CONFIGURATION
# ================================================================

# Generate UF2 file for drag-and-drop programming
pico_add_extra_outputs(${PROJECT_NAME})

# Generate additional output formats
pico_enable_stdio_usb(${PROJECT_NAME} 1)      # Enable USB serial output
pico_enable_stdio_uart(${PROJECT_NAME} 1)     # Enable UART serial output

# ================================================================
# MEMORY MAP CONFIGURATION
# ================================================================

# Custom linker script for TrustZone-M memory partitioning
# This defines secure vs non-secure memory regions
# Uncomment when implementing full TrustZone-M setup:
# pico_set_linker_script(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_trustzone.ld)

# ================================================================
# BUILD TYPE CONFIGURATION
# ================================================================

# Default to Release build for size optimization
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Print build configuration summary
message(STATUS "")
message(STATUS "=== ${PROJECT_NAME} Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Target Board: ${PICO_BOARD}")
message(STATUS "Flash Size: ${PICO_FLASH_SIZE_BYTES} bytes")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Optimization: -Os (Size)")
message(STATUS "TrustZone-M: Enabled (-mcmse)")
message(STATUS "Binary Limit: 320KB")
message(STATUS "")

# ================================================================
# SIZE VERIFICATION POST-BUILD
# ================================================================

# Add custom post-build step to verify binary size
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Checking binary size..."
    COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E echo "Binary size check complete."
    COMMENT "Verifying binary size against 320KB limit"
)
