cmake_minimum_required(VERSION 3.16)

# CMake configuration for ESP-IDF native build environments
if(DEFINED ENV{IDF_PATH})
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(esp32_coprocessor)
else()
    message(STATUS "ESP-IDF environment not detected. Use PlatformIO or source export.sh/export.bat from ESP-IDF.")
endif()
