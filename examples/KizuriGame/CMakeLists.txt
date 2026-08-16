# KizuriGame — o executável final de jogo. É o que transforma um projeto
# (cena .kzscene + GameModule com os scripts C++) num jogo que roda sozinho,
# sem o editor. Uso:
#
#   KizuriGame <cena.kzscene> [GameModule.ext]
#
# Sem argumentos, tenta "Start.kzscene" na pasta atual. Os caminhos de
# assets dentro da cena são relativos ao diretório de trabalho, então rode
# a partir da pasta do projeto/do jogo.
#
# Android: o mesmo jogo vira a lib libKizuriGame.so exportada pra Android —
# a entrada é android_main (AndroidEntry.cpp, via android_native_app_glue),
# que espera a primeira ANativeWindow e então constrói a mesma GameApp.
if(ANDROID)
    # native_app_glue do NDK (app glue lifecycle/touch) — o glue é C e
    # precisa do caminho do NDK (CMAKE_ANDROID_NDK é setado pela toolchain).
    if(NOT DEFINED CMAKE_ANDROID_NDK)
        set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
    endif()
    set(KZ_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    add_library(native_app_glue STATIC
        ${KZ_GLUE_DIR}/android_native_app_glue.c)
    target_include_directories(native_app_glue PUBLIC ${KZ_GLUE_DIR})

    add_library(KizuriGame SHARED
        src/GameApp.cpp
        src/AndroidEntry.cpp
    )
    target_include_directories(KizuriGame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # GameApp.cpp inclui <imgui.h> (sem usar funções); no Android não há
    # target imgui (só headers do FetchContent).
    target_include_directories(KizuriGame PRIVATE ${imgui_SOURCE_DIR})
    target_link_libraries(KizuriGame PRIVATE KizuriEngine native_app_glue EGL android log)
    set_target_properties(KizuriGame PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
else()
    add_executable(KizuriGame src/GameApp.cpp
        $<$<BOOL:${WIN32}>:${CMAKE_CURRENT_SOURCE_DIR}/../../editor/resources/kizuri-editor.rc>
    )
    target_link_libraries(KizuriGame PRIVATE KizuriEngine)
    target_include_directories(KizuriGame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    set_target_properties(KizuriGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND COMMAND kz_copy_mingw_runtime)
        kz_copy_mingw_runtime(KizuriGame)
    endif()

    if(WIN32)
        target_link_options(KizuriGame PRIVATE $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-mwindows>)
        set_target_properties(KizuriGame PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    endif()
endif()

# No Windows, esconde o console em builds Release (comportamento de produto
# final "limpo" — igual ao Sandbox e ao Editor; mensagens vão pro log).
if(NOT MSVC)
    target_compile_options(KizuriGame PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unknown-warning-option)
endif()
