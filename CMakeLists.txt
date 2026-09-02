cmake_minimum_required(VERSION 3.16)
project(bg_remover VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(INSTALLED "Build for installed location" OFF)
if(INSTALLED)
    add_definitions(-DINSTALLED)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(ONNX_DIR "${CMAKE_SOURCE_DIR}/onnxruntime-linux-x64-1.19.0")
include_directories("${ONNX_DIR}/include")

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNX_DIR}/lib/libonnxruntime.so"
)

# ---- GUI ----
add_executable(bg_remover_gui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    maskeditcanvas.cpp
    maskeditcanvas.h
    backgroundremover.cpp
    backgroundremover.h)
target_link_libraries(bg_remover_gui
    Qt6::Core Qt6::Gui Qt6::Widgets onnxruntime)
set_target_properties(bg_remover_gui PROPERTIES
    INSTALL_RPATH "/usr/lib/bg-remover"
    BUILD_RPATH "${ONNX_DIR}/lib"
)

# ---- CLI batch ----
add_executable(bg_remover_cli
    main_cli.cpp
    batchprocessor.cpp
    batchprocessor.h
    backgroundremover.cpp
    backgroundremover.h)
target_link_libraries(bg_remover_cli
    Qt6::Core Qt6::Gui onnxruntime)
set_target_properties(bg_remover_cli PROPERTIES
    INSTALL_RPATH "/usr/lib/bg-remover"
    BUILD_RPATH "${ONNX_DIR}/lib"
)

# ---- teste ----
enable_testing()
add_executable(test_inference
    test_inference.cpp
    backgroundremover.cpp
    backgroundremover.h)
target_link_libraries(test_inference
    Qt6::Core Qt6::Gui onnxruntime)
add_test(NAME inference_test
    COMMAND test_inference "${CMAKE_SOURCE_DIR}/u2netp.onnx")

# Verificacao preventiva: script separado evita escaping no CMake
configure_file(
    "${CMAKE_SOURCE_DIR}/check_m5_functions.sh.in"
    "${CMAKE_BINARY_DIR}/check_m5_functions.sh"
    @ONLY)
add_custom_command(TARGET bg_remover_gui PRE_BUILD
    COMMAND bash "${CMAKE_BINARY_DIR}/check_m5_functions.sh"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Verificando funcoes M5..."
)

# ---- instalacoes ----
install(TARGETS bg_remover_gui bg_remover_cli
    RUNTIME DESTINATION bin)

file(GLOB ONNX_RUNTIME_LIBS
    "${ONNX_DIR}/lib/libonnxruntime.so*")
install(FILES ${ONNX_RUNTIME_LIBS}
    DESTINATION lib/bg-remover)

install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx"
    DESTINATION share/bg-remover)
install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx"
    DESTINATION share/bg-remover)
install(FILES "${CMAKE_SOURCE_DIR}/bg-remover.desktop"
    DESTINATION share/applications)
install(FILES "${CMAKE_SOURCE_DIR}/bg-remover.svg"
    DESTINATION share/icons/hicolor/scalable/apps)

# ---- CPack ----
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "bg-remover")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "theking1x")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Background remover using U²-Netp")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Removes backgrounds from images using the U²-Netp or "
    "U²-Net ONNX models. GUI and CLI batch processing.")
set(CPACK_STRIP_FILES ON)
include(CPack)
