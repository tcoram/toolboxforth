cmake_minimum_required(VERSION 3.13)

# initialize the SDK directly
include(pico_sdk_import.cmake)

project(my_project)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(toolboxforth
    toolboxforth.c
    ../tbforth.c
)

pico_enable_stdio_usb(toolboxforth 1)
pico_enable_stdio_uart(toolboxforth 0)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(toolboxforth pico_stdlib hardware_flash)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(toolboxforth)
