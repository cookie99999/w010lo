cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2)
include(pico_sdk_import.cmake)

project(vga-controller C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(vga-controller
  vga-controller.c vga_graphics.c
)

pico_generate_pio_header(vga-controller ${CMAKE_CURRENT_LIST_DIR}/dbus.pio)
pico_generate_pio_header(vga-controller ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(vga-controller ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(vga-controller ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)
pico_enable_stdio_usb(vga-controller 1)
pico_enable_stdio_uart(vga-controller 0)

pico_add_extra_outputs(vga-controller)

target_link_libraries(vga-controller m pico_stdlib hardware_pio hardware_dma hardware_irq)
