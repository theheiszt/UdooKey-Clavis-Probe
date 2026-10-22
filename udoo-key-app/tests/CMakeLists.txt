cmake_minimum_required(VERSION 3.20)

add_executable(udoo_key_tests
    test_main.cpp
    test_ring_buffer.cpp
    test_stream_demux.cpp
    test_binary_logic.cpp
    test_sump_client.cpp
    test_decoders.cpp
    test_exporters.cpp
    test_jtag_swd.cpp
    test_architecture_and_usb.cpp
    test_self_test.cpp
    test_gpio_client.cpp
    test_mode_client.cpp
    test_bus_tools_client.cpp
    test_syntax_client.cpp
    test_wireless_client.cpp
    test_packet_pcap.cpp
    test_esp32_discovery.cpp
    ${LIB_SOURCES}
    ${LIB_HEADERS}
)

target_link_libraries(udoo_key_tests PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Concurrent
    Qt6::Network
)

target_include_directories(udoo_key_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/device
    ${CMAKE_SOURCE_DIR}/src/protocol
    ${CMAKE_SOURCE_DIR}/src/acquisition
    ${CMAKE_SOURCE_DIR}/src/decoders
    ${CMAKE_SOURCE_DIR}/src/export
    ${CMAKE_SOURCE_DIR}/src/debug
    ${CMAKE_SOURCE_DIR}/src/usb
    ${CMAKE_SOURCE_DIR}/src/network
    ${CMAKE_SOURCE_DIR}/../shared
)

add_test(NAME udoo_key_tests COMMAND udoo_key_tests)

set(UI_SOURCES_FULL "")
foreach(f ${UI_SOURCES})
    list(APPEND UI_SOURCES_FULL "${CMAKE_SOURCE_DIR}/${f}")
endforeach()

set(UI_HEADERS_FULL "")
foreach(f ${UI_HEADERS})
    list(APPEND UI_HEADERS_FULL "${CMAKE_SOURCE_DIR}/${f}")
endforeach()

add_executable(udoo_key_functional_test
    test_linux_functional.cpp
    ${UI_SOURCES_FULL}
    ${UI_HEADERS_FULL}
    ${LIB_SOURCES}
    ${LIB_HEADERS}
    ${CMAKE_SOURCE_DIR}/resources.qrc
)

target_link_libraries(udoo_key_functional_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Concurrent
    Qt6::Network
)

if(TARGET Qt6::Svg)
    target_link_libraries(udoo_key_functional_test PRIVATE Qt6::Svg Qt6::SvgWidgets)
endif()

target_include_directories(udoo_key_functional_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/device
    ${CMAKE_SOURCE_DIR}/src/protocol
    ${CMAKE_SOURCE_DIR}/src/acquisition
    ${CMAKE_SOURCE_DIR}/src/decoders
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/widgets
    ${CMAKE_SOURCE_DIR}/src/ui/views
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs
    ${CMAKE_SOURCE_DIR}/src/export
    ${CMAKE_SOURCE_DIR}/src/debug
    ${CMAKE_SOURCE_DIR}/src/usb
    ${CMAKE_SOURCE_DIR}/src/network
    ${CMAKE_SOURCE_DIR}/../shared
)

add_test(NAME udoo_key_functional_test COMMAND udoo_key_functional_test)
