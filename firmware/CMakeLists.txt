cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(speak5220 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(speak5220
    src/main.c
    src/tms5220.c
    src/tms6100.c
)

target_include_directories(speak5220 PRIVATE src)
target_compile_options(speak5220 PRIVATE -Wall -Wextra)

target_link_libraries(speak5220
    pico_stdlib
    pico_multicore
)

# GPIO0/1 are the TMS5220 data bus, so stdio must use USB only
pico_enable_stdio_usb(speak5220 1)
pico_enable_stdio_uart(speak5220 0)

pico_add_extra_outputs(speak5220)
