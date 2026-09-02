cmake_minimum_required(VERSION 3.16)
project(bg_remover VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Opcao: build para instalacao (caminhos do sistema)
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

# ---- binario principal ----
add_executable(bg_remover_gui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    backgroundremover.cpp
    backgroundremover.h)

target_link_libraries(bg_remover_gui
    Qt6::Core Qt6::Gui Qt6::Widgets onnxruntime)

set_target_properties(bg_remover_gui PROPERTIES
    INSTALL_RPATH "/usr/lib/bg-remover"
    BUILD_RPATH "${ONNX_DIR}/lib"
)

# ---- teste ----
enable_testing()

add_executable(test_inference test_inference.cpp
    backgroundremover.cpp
    backgroundremover.h)
target_link_libraries(test_inference
    Qt6::Core Qt6::Gui onnxruntime)

add_test(NAME inference_test
    COMMAND test_inference "${CMAKE_SOURCE_DIR}/u2netp.onnx")

# ---- regras de instalacao ----
install(TARGETS bg_remover_gui
    RUNTIME DESTINATION bin
)

# BUGFIX (M6): o binario esta linkado contra a SONAME
# libonnxruntime.so.1 (nao libonnxruntime.so). Instalar so o
# symlink sem versao perde o nome que o loader realmente procura
# em runtime. GLOB pega toda a cadeia (libonnxruntime.so,
# .so.1, .so.1.19.0) e instala todos preservando os nomes.
file(GLOB ONNXRUNTIME_LIBS "${ONNX_DIR}/lib/libonnxruntime.so*")
install(FILES ${ONNXRUNTIME_LIBS}
    DESTINATION lib/bg-remover)

install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx"
    DESTINATION share/bg-remover)

install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx"
    DESTINATION share/bg-remover)

install(FILES "${CMAKE_SOURCE_DIR}/bg-remover.desktop"
    DESTINATION share/applications)

install(FILES "${CMAKE_SOURCE_DIR}/bg-remover.svg"
    DESTINATION share/icons/hicolor/scalable/apps)

# ---- CPack (.deb) ----
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
    "U²-Net ONNX models. Includes GUI and batch processing.")
set(CPACK_STRIP_FILES ON)

include(CPack)
