cmake_minimum_required(VERSION 3.20)
project(SimpleRFCOMM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and optimizations
if(MSVC)
    add_compile_options(/W4 /EHsc /O2 /permissive-)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Core sources
set(SIMPLERFCOMM_SOURCES
    src/socket.cpp
    src/server.cpp
    src/client.cpp
    src/serial_port.cpp
)

# Platform specific SDP implementation and libraries
if(WIN32)
    list(APPEND SIMPLERFCOMM_SOURCES src/sdp_win.cpp)
    set(PLATFORM_LIBS ws2_32 bthprops)
elseif(UNIX)
    list(APPEND SIMPLERFCOMM_SOURCES src/sdp_linux.cpp)
    find_library(BLUETOOTH_LIB NAMES bluetooth)
    if(NOT BLUETOOTH_LIB)
        message(FATAL_ERROR "libbluetooth not found! Please install libbluetooth-dev (e.g. sudo apt install libbluetooth-dev)")
    endif()
    set(PLATFORM_LIBS ${BLUETOOTH_LIB})
endif()

# SimpleRFCOMM Library
add_library(simple_rfcomm STATIC ${SIMPLERFCOMM_SOURCES})

target_include_directories(simple_rfcomm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(simple_rfcomm PUBLIC ${PLATFORM_LIBS})

# Examples
add_executable(server_bench examples/server_bench.cpp)
target_link_libraries(server_bench PRIVATE simple_rfcomm)

add_executable(client_bench examples/client_bench.cpp)
target_link_libraries(client_bench PRIVATE simple_rfcomm)

add_executable(chat_echo examples/chat_echo.cpp)
target_link_libraries(chat_echo PRIVATE simple_rfcomm)

add_executable(serial_port_demo examples/serial_port_demo.cpp)
target_link_libraries(serial_port_demo PRIVATE simple_rfcomm)

# Installation support
include(GNUInstallDirs)
install(TARGETS simple_rfcomm server_bench client_bench chat_echo serial_port_demo
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
