cmake_minimum_required(VERSION 3.13)

# Include Pico SDK setup
include(pico_sdk_import.cmake OPTIONAL)

project(rp2040_hid_engine C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED PICO_SDK_PATH)
    message(WARNING "PICO_SDK_PATH not set! Only relevant if building natively with pico-sdk instead of PlatformIO.")
endif()

# Initialize the Pico SDK
if (COMMAND pico_sdk_init)
    pico_sdk_init()

    add_executable(rp2040_hid_engine
        src/main.cpp
    )

    target_include_directories(rp2040_hid_engine PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/../shared
    )

    target_link_libraries(rp2040_hid_engine
        pico_stdlib
        tinyusb_device
        tinyusb_board
        hardware_uart
    )

    pico_enable_stdio_usb(rp2040_hid_engine 1)
    pico_enable_stdio_uart(rp2040_hid_engine 0)

    pico_add_extra_outputs(rp2040_hid_engine)
endif()
