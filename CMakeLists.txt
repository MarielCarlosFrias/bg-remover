cmake_minimum_required(VERSION 3.16)
project(u2netp_cli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

set(ONNX_DIR "${CMAKE_SOURCE_DIR}/onnxruntime-linux-x64-1.19.0")
include_directories("${ONNX_DIR}/include")

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNX_DIR}/lib/libonnxruntime.so"
)

add_executable(u2netp_cli main.cpp backgroundremover.cpp)
target_link_libraries(u2netp_cli Qt6::Core Qt6::Gui onnxruntime)

set_target_properties(u2netp_cli PROPERTIES
    BUILD_RPATH "${ONNX_DIR}/lib"
    INSTALL_RPATH "${ONNX_DIR}/lib"
)
