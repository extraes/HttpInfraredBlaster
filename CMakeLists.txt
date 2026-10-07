# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(blink C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(blink
        blink.c
        wifi_http_server.c
)

pico_set_program_name(blink "blink")
pico_set_program_version(blink "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(blink 1)
pico_enable_stdio_usb(blink 0)

# Add the standard library to the build
target_link_libraries(blink
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(blink PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(blink 
        hardware_timer
        hardware_clocks
        hardware_pwm
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mdns
        )

set(WIFI_SSID "" CACHE STRING "Wi-Fi network name")
set(WIFI_PASSWORD "" CACHE STRING "Wi-Fi network password")
set(MDNS_HOSTNAME "infrared" CACHE STRING "Advertised as <name>.local")
target_compile_definitions(blink PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        MDNS_HOSTNAME=\"${MDNS_HOSTNAME}\"
        CYW43_HOST_NAME=\"${MDNS_HOSTNAME}\"
        ACTIVITY_LED_PIN=${ACTIVITY_LED_PIN}
)

pico_add_extra_outputs(blink)
