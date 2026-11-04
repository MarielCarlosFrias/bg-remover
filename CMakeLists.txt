cmake_minimum_required(VERSION 3.16)
project(aletheia VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(INSTALLED "Build for installed location" OFF)
option(BG_REMOVER_FETCH_ONNX_RUNTIME
    "Download the pinned ONNX Runtime dependency when it is missing" OFF)
option(BG_REMOVER_BUNDLE_MODELS
    "Require and include bundled ONNX models in the install package" OFF)
option(BG_REMOVER_WARNINGS
    "Enable common compiler warnings (recommended for development)" OFF)
if(INSTALLED)
    add_definitions(-DINSTALLED)
endif()
if(BG_REMOVER_WARNINGS)
    if(MSVC)
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)

set(ONNX_RUNTIME_VERSION "1.19.0")
if(WIN32)
    set(ONNX_RUNTIME_ARCHIVE
        "onnxruntime-win-x64-${ONNX_RUNTIME_VERSION}.zip")
    set(ONNX_RUNTIME_SHA256
        "1d796da7001e4843858d0587aa8232976abf9e0ae7fba8deb7fa8156e440efb7")
elseif(APPLE)
    # Binario universal (x86_64 + arm64): o mesmo pacote corre em Macs
    # Intel e Apple Silicon.
    set(ONNX_RUNTIME_ARCHIVE
        "onnxruntime-osx-universal2-${ONNX_RUNTIME_VERSION}.tgz")
    set(ONNX_RUNTIME_SHA256
        "a40293ad13c166289a83b072421faa7883b5ae4694ad93d6445ac32bd08be6ab")
else()
    set(ONNX_RUNTIME_ARCHIVE
        "onnxruntime-linux-x64-${ONNX_RUNTIME_VERSION}.tgz")
    set(ONNX_RUNTIME_SHA256
        "8627cae49b51544ed15bb70c8ecf25b187f22a301c99c1ced5f4f3af5c36b44c")
endif()
string(REPLACE ".zip" "" ONNX_RUNTIME_DIR_NAME "${ONNX_RUNTIME_ARCHIVE}")
string(REPLACE ".tgz" "" ONNX_RUNTIME_DIR_NAME "${ONNX_RUNTIME_DIR_NAME}")
set(BG_REMOVER_ONNX_RUNTIME_DIR
    "${CMAKE_SOURCE_DIR}/${ONNX_RUNTIME_DIR_NAME}" CACHE PATH
    "Path to an extracted ONNX Runtime distribution")
set(ONNX_DIR "${BG_REMOVER_ONNX_RUNTIME_DIR}")

if(NOT EXISTS "${ONNX_DIR}/include/onnxruntime_cxx_api.h"
   AND BG_REMOVER_FETCH_ONNX_RUNTIME)
    set(ONNX_RUNTIME_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps")
    set(ONNX_RUNTIME_DOWNLOAD_FILE
        "${ONNX_RUNTIME_DOWNLOAD_DIR}/${ONNX_RUNTIME_ARCHIVE}")
    file(MAKE_DIRECTORY "${ONNX_RUNTIME_DOWNLOAD_DIR}")
    file(DOWNLOAD
        "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_RUNTIME_VERSION}/${ONNX_RUNTIME_ARCHIVE}"
        "${ONNX_RUNTIME_DOWNLOAD_FILE}"
        TLS_VERIFY ON
        EXPECTED_HASH "SHA256=${ONNX_RUNTIME_SHA256}"
        STATUS ONNX_RUNTIME_DOWNLOAD_STATUS
        SHOW_PROGRESS)
    list(GET ONNX_RUNTIME_DOWNLOAD_STATUS 0 ONNX_RUNTIME_DOWNLOAD_CODE)
    list(GET ONNX_RUNTIME_DOWNLOAD_STATUS 1 ONNX_RUNTIME_DOWNLOAD_MESSAGE)
    if(NOT ONNX_RUNTIME_DOWNLOAD_CODE EQUAL 0)
        message(FATAL_ERROR
            "Falha ao baixar ONNX Runtime: ${ONNX_RUNTIME_DOWNLOAD_MESSAGE}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${ONNX_RUNTIME_DOWNLOAD_FILE}"
         DESTINATION "${ONNX_RUNTIME_DOWNLOAD_DIR}")
    set(ONNX_DIR "${ONNX_RUNTIME_DOWNLOAD_DIR}/${ONNX_RUNTIME_DIR_NAME}")
endif()

if(BG_REMOVER_BUNDLE_MODELS AND NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    message(FATAL_ERROR
        "BG_REMOVER_BUNDLE_MODELS esta ativo, mas u2netp.onnx nao existe. "
        "Baixe o modelo antes de gerar o pacote.")
endif()
include_directories(
    "${ONNX_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}/src/gui"
)

if(NOT EXISTS "${ONNX_DIR}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR
        "ONNX Runtime nao encontrado em ${ONNX_DIR}. "
        "Baixe e extraia a versao indicada no README.")
endif()

add_library(onnxruntime SHARED IMPORTED)
if(WIN32)
    set(ONNX_RUNTIME_LIBRARY "${ONNX_DIR}/lib/onnxruntime.dll")
    set(ONNX_RUNTIME_PROVIDER_LIBRARY
        "${ONNX_DIR}/lib/onnxruntime_providers_shared.dll")
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_RUNTIME_LIBRARY}"
        IMPORTED_IMPLIB "${ONNX_DIR}/lib/onnxruntime.lib"
    )
elseif(APPLE)
    set(ONNX_RUNTIME_LIBRARY "${ONNX_DIR}/lib/libonnxruntime.dylib")
    set(ONNX_RUNTIME_DYLIB
        "${ONNX_DIR}/lib/libonnxruntime.${ONNX_RUNTIME_VERSION}.dylib")
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_RUNTIME_LIBRARY}"
    )
else()
    set(ONNX_RUNTIME_LIBRARY "${ONNX_DIR}/lib/libonnxruntime.so")
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_RUNTIME_LIBRARY}"
    )
endif()
if(NOT EXISTS "${ONNX_RUNTIME_LIBRARY}")
    message(FATAL_ERROR
        "Biblioteca ONNX Runtime nao encontrada: ${ONNX_RUNTIME_LIBRARY}")
endif()
if(WIN32 AND NOT EXISTS "${ONNX_RUNTIME_PROVIDER_LIBRARY}")
    message(FATAL_ERROR
        "Biblioteca auxiliar ONNX Runtime nao encontrada: "
        "${ONNX_RUNTIME_PROVIDER_LIBRARY}")
endif()

add_executable(aletheia
    src/gui/main.cpp
    src/gui/mainwindow.cpp
    src/gui/mainwindow.h
    src/gui/maskeditcanvas.cpp
    src/gui/maskeditcanvas.h
    src/gui/helpwidget.cpp
    src/gui/helpwidget.h
    src/gui/apptheme.cpp
    src/gui/apptheme.h
    src/core/i18n.h
    src/core/imagepostprocessor.cpp
    src/core/imagepostprocessor.h
    src/core/modelmanager.cpp
    src/core/modelmanager.h
    src/core/modelpathresolver.cpp
    src/core/modelpathresolver.h
    src/core/modelvalidator.cpp
    src/core/modelvalidator.h
    src/core/processingoptions.h
    src/core/batchplan.h
    src/core/backgroundremover.cpp
    src/core/backgroundremover.h)
target_link_libraries(aletheia
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent
    onnxruntime)
target_compile_definitions(aletheia PRIVATE
    BG_REMOVER_VERSION="${PROJECT_VERSION}")
if(APPLE)
    # App bundle nativo: o executavel fica em Contents/MacOS e as
    # bibliotecas (ONNX + Qt via macdeployqt) em Contents/Frameworks.
    set_target_properties(aletheia PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Aletheia"
        MACOSX_BUNDLE_GUI_IDENTIFIER
            "io.github.MarielCarlosFrias.Aletheia"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
elseif(NOT WIN32)
    set_target_properties(aletheia PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib/aletheia"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
endif()

add_executable(aletheia_cli
    src/cli/main_cli.cpp
    src/core/i18n.h
    src/core/batchprocessor.cpp
    src/core/batchprocessor.h
    src/core/batchplan.h
    src/core/imagepostprocessor.cpp
    src/core/imagepostprocessor.h
    src/core/modelpathresolver.cpp
    src/core/modelpathresolver.h
    src/core/processingoptions.h
    src/core/backgroundremover.cpp
    src/core/backgroundremover.h)
target_link_libraries(aletheia_cli
    Qt6::Core Qt6::Gui onnxruntime)
target_compile_definitions(aletheia_cli PRIVATE
    BG_REMOVER_VERSION="${PROJECT_VERSION}")
if(APPLE)
    # O CLI e instalado dentro do .app para partilhar os frameworks
    # Qt/ONNX e o modelo incluido em Contents/Resources.
    set_target_properties(aletheia_cli PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
elseif(NOT WIN32)
    set_target_properties(aletheia_cli PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib/aletheia"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
endif()

if(WIN32)
    add_custom_command(TARGET aletheia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:aletheia>)
    add_custom_command(TARGET aletheia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_RUNTIME_PROVIDER_LIBRARY}"
            $<TARGET_FILE_DIR:aletheia>)
    add_custom_command(TARGET aletheia_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:aletheia_cli>)
    add_custom_command(TARGET aletheia_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_RUNTIME_PROVIDER_LIBRARY}"
            $<TARGET_FILE_DIR:aletheia_cli>)
endif()

enable_testing()
add_executable(test_postprocessing
    tests/test_postprocessing.cpp
    src/core/imagepostprocessor.cpp
    src/core/imagepostprocessor.h
    src/core/processingoptions.h)
target_link_libraries(test_postprocessing Qt6::Core Qt6::Gui)
add_test(NAME postprocessing_test COMMAND test_postprocessing)

add_executable(test_batchplan
    tests/test_batchplan.cpp
    src/core/batchplan.h)
target_link_libraries(test_batchplan Qt6::Core)
add_test(NAME batchplan_test COMMAND test_batchplan)

add_executable(test_modelpath
    tests/test_modelpath.cpp
    src/core/modelpathresolver.cpp
    src/core/modelpathresolver.h)
target_link_libraries(test_modelpath Qt6::Core)
add_test(NAME modelpath_test COMMAND test_modelpath)

add_executable(test_modelvalidator
    tests/test_modelvalidator.cpp
    src/core/modelvalidator.cpp
    src/core/modelvalidator.h)
target_link_libraries(test_modelvalidator Qt6::Core)
add_test(NAME modelvalidator_test COMMAND test_modelvalidator)

add_executable(test_savepath
    tests/test_savepath.cpp
    src/core/savepath.h)
target_link_libraries(test_savepath Qt6::Core)
add_test(NAME savepath_test COMMAND test_savepath)

add_executable(test_imageformats
    tests/test_imageformats.cpp)
target_link_libraries(test_imageformats Qt6::Core Qt6::Gui)
add_test(NAME imageformats_test COMMAND test_imageformats)

# Teste de fumo da GUI: constroi a janela real em modo offscreen,
# abre imagens, verifica atalhos/controlos e (quando o modelo existe)
# corre uma inferencia completa. Sem modelo, o caso de inferencia
# valida apenas a recuperacao da UI.
add_executable(test_gui
    tests/test_gui.cpp
    src/gui/mainwindow.cpp
    src/gui/mainwindow.h
    src/gui/maskeditcanvas.cpp
    src/gui/maskeditcanvas.h
    src/gui/helpwidget.cpp
    src/gui/helpwidget.h
    src/gui/apptheme.cpp
    src/gui/apptheme.h
    src/core/i18n.h
    src/core/imagepostprocessor.cpp
    src/core/imagepostprocessor.h
    src/core/modelmanager.cpp
    src/core/modelmanager.h
    src/core/modelpathresolver.cpp
    src/core/modelpathresolver.h
    src/core/modelvalidator.cpp
    src/core/modelvalidator.h
    src/core/processingoptions.h
    src/core/batchplan.h
    src/core/backgroundremover.cpp
    src/core/backgroundremover.h)
target_link_libraries(test_gui
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent
    onnxruntime)
target_compile_definitions(test_gui PRIVATE
    BG_REMOVER_VERSION="${PROJECT_VERSION}")
if(NOT WIN32)
    set_target_properties(test_gui PROPERTIES
        BUILD_RPATH "${ONNX_DIR}/lib")
endif()
add_test(NAME gui_test_pt
    COMMAND test_gui "${CMAKE_SOURCE_DIR}/u2netp.onnx")
add_test(NAME gui_test_en
    COMMAND test_gui "${CMAKE_SOURCE_DIR}/u2netp.onnx")
set_tests_properties(gui_test_pt PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;BG_REMOVER_LANG=pt;BG_REMOVER_NO_UPDATE_CHECK=1")
set_tests_properties(gui_test_en PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;BG_REMOVER_LANG=en;BG_REMOVER_NO_UPDATE_CHECK=1")

add_executable(test_batchprocessor
    tests/test_batchprocessor.cpp
    src/core/batchprocessor.cpp
    src/core/batchprocessor.h
    src/core/batchplan.h
    src/core/imagepostprocessor.cpp
    src/core/imagepostprocessor.h
    src/core/processingoptions.h
    src/core/backgroundremover.cpp
    src/core/backgroundremover.h
    src/core/i18n.h)
target_link_libraries(test_batchprocessor
    Qt6::Core Qt6::Gui onnxruntime)
add_test(NAME batchprocessor_test
    COMMAND test_batchprocessor "${CMAKE_SOURCE_DIR}/u2netp.onnx")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    set_tests_properties(batchprocessor_test PROPERTIES DISABLED TRUE)
    message(STATUS "Teste do lote desativado: u2netp.onnx nao encontrado")
endif()

add_executable(test_inference
    tests/test_inference.cpp
    src/core/backgroundremover.cpp
    src/core/backgroundremover.h)
target_link_libraries(test_inference
    Qt6::Core Qt6::Gui onnxruntime)
add_test(NAME inference_test
    COMMAND test_inference "${CMAKE_SOURCE_DIR}/u2netp.onnx")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    set_tests_properties(inference_test PROPERTIES DISABLED TRUE)
    message(STATUS "Teste de inferencia desativado: u2netp.onnx nao encontrado")
endif()

if(WIN32)
    install(TARGETS aletheia aletheia_cli RUNTIME DESTINATION .)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION .)
    install(FILES "${ONNX_DIR}/LICENSE"
        DESTINATION . RENAME LICENSE.onnxruntime OPTIONAL)
    install(FILES "${ONNX_DIR}/lib/onnxruntime.dll" DESTINATION .)
    install(FILES "${ONNX_RUNTIME_PROVIDER_LIBRARY}" DESTINATION .)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    set(WINDEPLOYQT_EXECUTABLE "${QT_BIN_DIR}/windeployqt.exe")
    if(NOT EXISTS "${WINDEPLOYQT_EXECUTABLE}")
        message(FATAL_ERROR "windeployqt.exe nao encontrado: ${WINDEPLOYQT_EXECUTABLE}")
    endif()
    install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations --compiler-runtime \"\${CMAKE_INSTALL_PREFIX}/aletheia.exe\" RESULT_VARIABLE deploy_result)\nif(NOT deploy_result EQUAL 0)\n  message(FATAL_ERROR \"windeployqt falhou: \${deploy_result}\")\nendif()")

    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx" DESTINATION .)
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx" DESTINATION . OPTIONAL)
    endif()

    set(CPACK_GENERATOR "ZIP")
    # ZIP plano: exes e DLLs na raiz do arquivo (extrair -> executar).
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
    set(CPACK_PACKAGE_NAME "aletheia")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_win64")
elseif(APPLE)
    # Bundle arrastavel para /Applications. O CLI fica dentro do .app
    # para reutilizar os frameworks e o modelo, sem uma segunda copia
    # de Qt/ONNX no ZIP.
    install(TARGETS aletheia BUNDLE DESTINATION .)
    install(TARGETS aletheia_cli
        RUNTIME DESTINATION "Aletheia.app/Contents/MacOS")
    install(FILES "${ONNX_RUNTIME_DYLIB}"
        DESTINATION "Aletheia.app/Contents/Frameworks")
    install(CODE "
        execute_process(COMMAND \"${CMAKE_COMMAND}\" -E remove -f
            \"\${CMAKE_INSTALL_PREFIX}/Aletheia.app/Contents/Frameworks/libonnxruntime.dylib\")
        execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
            \"libonnxruntime.${ONNX_RUNTIME_VERSION}.dylib\"
            \"\${CMAKE_INSTALL_PREFIX}/Aletheia.app/Contents/Frameworks/libonnxruntime.dylib\")
    ")
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "Aletheia.app/Contents/Resources"
        RENAME LICENSE.txt)
    install(FILES "${ONNX_DIR}/LICENSE"
        DESTINATION "Aletheia.app/Contents/Resources"
        RENAME LICENSE.onnxruntime.txt OPTIONAL)
    install(FILES "${ONNX_DIR}/ThirdPartyNotices.txt"
        DESTINATION "Aletheia.app/Contents/Resources" OPTIONAL)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.svg"
        DESTINATION "Aletheia.app/Contents/Resources")
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia_cli.1"
        DESTINATION share/man/man1)
    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx"
            DESTINATION "Aletheia.app/Contents/Resources")
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx"
            DESTINATION "Aletheia.app/Contents/Resources" OPTIONAL)
    endif()

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
    set(CPACK_PACKAGE_NAME "aletheia")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_macos_universal2")
else()
    install(TARGETS aletheia aletheia_cli
        RUNTIME DESTINATION bin)
    file(GLOB ONNX_RUNTIME_LIBS
        "${ONNX_DIR}/lib/libonnxruntime.so*")
    install(FILES ${ONNX_RUNTIME_LIBS}
        DESTINATION lib/aletheia)
    install(FILES "${ONNX_DIR}/LICENSE"
        DESTINATION share/doc/aletheia
        RENAME LICENSE.onnxruntime OPTIONAL)
    install(FILES "${ONNX_DIR}/ThirdPartyNotices.txt"
        DESTINATION share/doc/aletheia OPTIONAL)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION share/doc/aletheia)
    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx"
            DESTINATION share/aletheia)
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx"
            DESTINATION share/aletheia OPTIONAL)
    endif()
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.desktop"
        DESTINATION share/applications)
    install(FILES
        "${CMAKE_SOURCE_DIR}/assets/io.github.MarielCarlosFrias.Aletheia.metainfo.xml"
        DESTINATION share/metainfo)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.svg"
        DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia_cli.1"
        DESTINATION share/man/man1)

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "aletheia")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER
        "Mariel Carlos Frias <MarielCarlosFrias@users.noreply.github.com>")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE
        "https://github.com/MarielCarlosFrias/bg-remover")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6, libqt6network6")
    # WebP e opcional: sem o plugin o programa continua a funcionar
    # com PNG/JPEG e avisa o utilizador.
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-image-formats-plugins")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    # Uma unica string: varios argumentos viram uma lista CMake e o
    # CPack junta-os com ';' na descricao do pacote.
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Removes backgrounds from images using neural network ONNX models and Chroma Key. GUI and CLI batch processing.")
    set(CPACK_STRIP_FILES ON)
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Image background remover powered by AI and Chroma Key")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/;/.github/;/.codex/;/.agents/;/build[^/]*/;"
    "/onnxruntime-[^/]*/;/_rembg[^/]*/;/imagens[^/]*/;"
    "/.*\\.onnx$;/.*\\.bak$;/bg_remover_cli$;/aletheia$;/aletheia_cli$")

include(CPack)
