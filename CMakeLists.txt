cmake_minimum_required(VERSION 3.16)
project(bg_remover VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(INSTALLED "Build for installed location" OFF)
option(BG_REMOVER_FETCH_ONNX_RUNTIME
    "Download the pinned ONNX Runtime dependency when it is missing" OFF)
option(BG_REMOVER_BUNDLE_MODELS
    "Require and include bundled ONNX models in the install package" OFF)
if(INSTALLED)
    add_definitions(-DINSTALLED)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)

set(ONNX_RUNTIME_VERSION "1.19.0")
if(WIN32)
    set(ONNX_RUNTIME_ARCHIVE
        "onnxruntime-win-x64-${ONNX_RUNTIME_VERSION}.zip")
    set(ONNX_RUNTIME_SHA256
        "923aef63b600ae4686933aac380c307a99d0238e44d61678a67bcd188337faf7")
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
include_directories("${ONNX_DIR}/include")

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

add_executable(bg_remover_gui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    maskeditcanvas.cpp
    maskeditcanvas.h
    helpwidget.cpp
    helpwidget.h
    imagepostprocessor.cpp
    imagepostprocessor.h
    modelmanager.cpp
    modelmanager.h
    modelpathresolver.cpp
    modelpathresolver.h
    processingoptions.h
    apptheme.cpp
    apptheme.h
    batchplan.h
    backgroundremover.cpp
    backgroundremover.h)
target_link_libraries(bg_remover_gui
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent
    onnxruntime)
target_compile_definitions(bg_remover_gui PRIVATE
    BG_REMOVER_VERSION="${PROJECT_VERSION}")
if(NOT WIN32)
    set_target_properties(bg_remover_gui PROPERTIES
        INSTALL_RPATH "/usr/lib/bg-remover"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
endif()

add_executable(bg_remover_cli
    main_cli.cpp
    batchprocessor.cpp
    batchprocessor.h
    batchplan.h
    imagepostprocessor.cpp
    imagepostprocessor.h
    modelpathresolver.cpp
    modelpathresolver.h
    processingoptions.h
    backgroundremover.cpp
    backgroundremover.h)
target_link_libraries(bg_remover_cli
    Qt6::Core Qt6::Gui onnxruntime)
if(NOT WIN32)
    set_target_properties(bg_remover_cli PROPERTIES
        INSTALL_RPATH "/usr/lib/bg-remover"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
endif()

if(WIN32)
    add_custom_command(TARGET bg_remover_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:bg_remover_gui>)
    add_custom_command(TARGET bg_remover_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_RUNTIME_PROVIDER_LIBRARY}"
            $<TARGET_FILE_DIR:bg_remover_gui>)
    add_custom_command(TARGET bg_remover_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:bg_remover_cli>)
    add_custom_command(TARGET bg_remover_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_RUNTIME_PROVIDER_LIBRARY}"
            $<TARGET_FILE_DIR:bg_remover_cli>)
endif()

enable_testing()
add_executable(test_postprocessing
    test_postprocessing.cpp
    imagepostprocessor.cpp
    imagepostprocessor.h
    processingoptions.h)
target_link_libraries(test_postprocessing Qt6::Core Qt6::Gui)
add_test(NAME postprocessing_test COMMAND test_postprocessing)

add_executable(test_batchplan
    test_batchplan.cpp
    batchplan.h)
target_link_libraries(test_batchplan Qt6::Core)
add_test(NAME batchplan_test COMMAND test_batchplan)

add_executable(test_inference
    test_inference.cpp
    backgroundremover.cpp
    backgroundremover.h)
target_link_libraries(test_inference
    Qt6::Core Qt6::Gui onnxruntime)
add_test(NAME inference_test
    COMMAND test_inference "${CMAKE_SOURCE_DIR}/u2netp.onnx")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    set_tests_properties(inference_test PROPERTIES DISABLED TRUE)
    message(STATUS "Teste de inferencia desativado: u2netp.onnx nao encontrado")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/check_m5_functions.sh.in"
    "${CMAKE_BINARY_DIR}/check_m5_functions.sh"
    @ONLY)
if(NOT WIN32)
    add_custom_command(TARGET bg_remover_gui PRE_BUILD
        COMMAND bash "${CMAKE_BINARY_DIR}/check_m5_functions.sh"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Verificando funcoes M5..."
    )
endif()

if(WIN32)
    install(TARGETS bg_remover_gui bg_remover_cli RUNTIME DESTINATION .)
    install(FILES "${ONNX_DIR}/lib/onnxruntime.dll" DESTINATION .)
    install(FILES "${ONNX_RUNTIME_PROVIDER_LIBRARY}" DESTINATION .)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    set(WINDEPLOYQT_EXECUTABLE "${QT_BIN_DIR}/windeployqt.exe")
    if(NOT EXISTS "${WINDEPLOYQT_EXECUTABLE}")
        message(FATAL_ERROR "windeployqt.exe nao encontrado: ${WINDEPLOYQT_EXECUTABLE}")
    endif()
    install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations --compiler-runtime \"\${CMAKE_INSTALL_PREFIX}/bg_remover_gui.exe\" RESULT_VARIABLE deploy_result)\nif(NOT deploy_result EQUAL 0)\n  message(FATAL_ERROR \"windeployqt falhou: \${deploy_result}\")\nendif()")

    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx" DESTINATION .)
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx" DESTINATION . OPTIONAL)
    endif()

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "bg-remover")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_win64")
else()
    install(TARGETS bg_remover_gui bg_remover_cli
        RUNTIME DESTINATION bin)
    file(GLOB ONNX_RUNTIME_LIBS
        "${ONNX_DIR}/lib/libonnxruntime.so*")
    install(FILES ${ONNX_RUNTIME_LIBS}
        DESTINATION lib/bg-remover)
    install(FILES
        "${ONNX_DIR}/LICENSE"
        "${ONNX_DIR}/ThirdPartyNotices.txt"
        DESTINATION share/doc/bg-remover OPTIONAL)
    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx"
            DESTINATION share/bg-remover)
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx"
            DESTINATION share/bg-remover OPTIONAL)
    endif()
    install(FILES "${CMAKE_SOURCE_DIR}/bg-remover.desktop"
        DESTINATION share/applications)
    install(FILES "${CMAKE_SOURCE_DIR}/bg-remover.svg"
        DESTINATION share/icons/hicolor/scalable/apps)

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "bg-remover")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "theking1x")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Removes backgrounds from images using the U²-Netp or "
        "U²-Net ONNX models. GUI and CLI batch processing.")
    set(CPACK_STRIP_FILES ON)
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Background remover using U²-Netp")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/;/.github/;/.codex/;/.agents/;/build[^/]*/;"
    "/onnxruntime-[^/]*/;/_rembg[^/]*/;/imagens[^/]*/;"
    "/.*\\.onnx$;/.*\\.bak$;/bg_remover_cli$")

include(CPack)
