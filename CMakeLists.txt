cmake_minimum_required(VERSION 3.16)
project(bg_remover)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(ONNX_DIR "${CMAKE_SOURCE_DIR}/onnxruntime-linux-x64-1.19.0")
include_directories("${ONNX_DIR}/include")

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNX_DIR}/lib/libonnxruntime.so"
)

add_executable(bg_remover_gui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    backgroundremover.cpp
    backgroundremover.h)

target_link_libraries(bg_remover_gui
    Qt6::Core Qt6::Gui Qt6::Widgets onnxruntime)

set_target_properties(bg_remover_gui PROPERTIES
    BUILD_RPATH "${ONNX_DIR}/lib"
    INSTALL_RPATH "${ONNX_DIR}/lib"
)
