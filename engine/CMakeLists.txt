file(GLOB_RECURSE KIZURI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB_RECURSE KIZURI_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

# ---- glad (bindings gerados pelo glad2 para GL 4.5 core, comitados em
# third_party_generated/glad — ver README nessa pasta para como regerar) ----
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/third_party_generated/glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/third_party_generated/glad/include)

# ---- Fontes embutidas (JetBrains Mono) -------------------------------------
# Em vez de carregar os .ttf de um caminho relativo em tempo de execução (o
# que exige distribuir uma pasta assets/fonts junto do executável), cada
# fonte é convertida num array de bytes C++ em tempo de build e compilada
# direto na engine. O resultado final é um único executável autocontido.
set(KZ_EMBEDDED_FONTS_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/kizuri)
set(KZ_FONT_REGULAR_TTF ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/JetBrainsMono-Regular.ttf)
set(KZ_FONT_BOLD_TTF    ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/JetBrainsMono-Bold.ttf)
set(KZ_FONT_REGULAR_HPP ${KZ_EMBEDDED_FONTS_DIR}/EmbeddedFontRegular.hpp)
set(KZ_FONT_BOLD_HPP    ${KZ_EMBEDDED_FONTS_DIR}/EmbeddedFontBold.hpp)

add_custom_command(
    OUTPUT ${KZ_FONT_REGULAR_HPP}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${KZ_FONT_REGULAR_TTF}
        -DOUTPUT_FILE=${KZ_FONT_REGULAR_HPP}
        -DVAR_NAME=kFontRegularTTF
        -P ${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake
    DEPENDS ${KZ_FONT_REGULAR_TTF} ${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake
    COMMENT "Embutindo JetBrainsMono-Regular.ttf no executável"
)
add_custom_command(
    OUTPUT ${KZ_FONT_BOLD_HPP}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${KZ_FONT_BOLD_TTF}
        -DOUTPUT_FILE=${KZ_FONT_BOLD_HPP}
        -DVAR_NAME=kFontBoldTTF
        -P ${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake
    DEPENDS ${KZ_FONT_BOLD_TTF} ${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake
    COMMENT "Embutindo JetBrainsMono-Bold.ttf no executável"
)
add_custom_target(KizuriEmbeddedFonts DEPENDS ${KZ_FONT_REGULAR_HPP} ${KZ_FONT_BOLD_HPP})

# SHARED. Necessário pro roadmap (GameModule.dll carregado em runtime pelo
# editor precisa enxergar a MESMA instância da engine em memória — ver
# kizuri/scripting/ScriptEngine.hpp). Isso já tinha sido tentado antes e
# revertido pra STATIC porque não abria no hardware real — mas a causa
# raiz daquilo era uma flag `-static` aplicada global (sem restrição de
# alvo) no CMakeLists.txt da raiz, fazendo essa DLL e o KizuriEditor.exe
# embutirem cada um sua PRÓPRIA cópia separada do runtime C++ (heap,
# unwind de exceção). Isso foi corrigido na raiz: agora ninguém linka
# runtime estático, todo mundo compartilha a mesma libstdc++/libgcc/
# libwinpthread em runtime (kz_copy_mingw_runtime, definida lá), então não
# tem mais fronteira nenhuma pra std::string/std::shared_ptr/exceção
# quebrar ao atravessar DLL <-> EXE.
#
# AINDA NÃO TESTADO no hardware real depois dessa correção — só vira fato
# confirmado depois de alguém testar de novo (Winlator/Wine, Mali-G57).
# Se travar de novo, NÃO assumir de novo "SHARED não funciona" e voltar
# pra STATIC sem investigar: olhar KizuriEngine.log primeiro, e confirmar
# se libgcc_s_seh-1.dll/libstdc++-6.dll/libwinpthread-1.dll realmente
# foram parar em bin/ junto do .exe (kz_copy_mingw_runtime só funciona se
# achar essas DLLs do lado do compilador — pode falhar em setups atípicos
# de MinGW/MSYS2).
add_library(KizuriEngine SHARED ${KIZURI_SOURCES} ${KIZURI_HEADERS})
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND COMMAND kz_copy_mingw_runtime)
    kz_copy_mingw_runtime(KizuriEngine)
endif()

# O código da engine não tem macro de export por classe (__declspec
# (dllexport) em cada símbolo público) — o projeto não foi desenhado com
# isso desde o início, e adicionar retroativamente em toda classe seria
# grande demais pra valer a pena agora. WINDOWS_EXPORT_ALL_SYMBOLS faz o
# CMake gerar isso automaticamente (equivalente ao que Linux/macOS já
# fazem por padrão, onde todo símbolo público de uma lib compartilhada já
# é visível de fora sem anotação nenhuma).
set_target_properties(KizuriEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# WINDOWS_EXPORT_ALL_SYMBOLS é indiscriminado — exporta TODO símbolo global
# visível na hora de linkar a DLL, inclusive os que vieram de biblioteca
# estática embutida (libgcc_eh, de onde vem `_Unwind_Resume`; e também
# símbolos internos do Bullet3/Box2D etc). No MinGW isso faz a KizuriEngine
# "vazar" `_Unwind_Resume` como se fosse parte da própria API pública dela.
# Como o Sandbox/Editor TAMBÉM linkam sua própria cópia estática desse
# símbolo (via -static-libgcc), o linker via duas definições disponíveis
# pro mesmo símbolo e recusava — exatamente o "multiple definition of
# `_Unwind_Resume`" que apareceu no build.
# --exclude-libs,ALL resolve na raiz: só exporta símbolo que foi de fato
# DEFINIDO nos próprios arquivos-fonte da engine, nunca um que só passou
# de carona por uma biblioteca estática linkada junto.
#
# Nota: agora que ninguém mais linka -static-libgcc (ver CMakeLists.txt da
# raiz), o cenário específico de "multiple definition of `_Unwind_Resume`"
# descrito acima não deveria mais acontecer — mas a flag continua útil por
# conta própria (Bullet3/Box2D/imgui são libs estáticas embutidas na
# KizuriEngine.dll, e sem isso WINDOWS_EXPORT_ALL_SYMBOLS exportaria os
# símbolos internos delas também, inchando a API pública da DLL à toa).
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(KizuriEngine PRIVATE "-Wl,--exclude-libs,ALL")
endif()

add_dependencies(KizuriEngine KizuriEmbeddedFonts)

target_include_directories(KizuriEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${miniaudio_SOURCE_DIR}
        ${stb_SOURCE_DIR}
        ${tinyobjloader_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${bullet3_SOURCE_DIR}/src
        ${box2d_SOURCE_DIR}/include
    PRIVATE
        ${KZ_EMBEDDED_FONTS_DIR}
)

# ---- ImGui (compilado direto como parte da engine, com backend GLFW+GL3) --
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw glad)
# Evita que imgui_impl_glfw.cpp arraste GL/gl.h e GL/glext.h do sistema
# (que conflitam com o KHR/khrplatform.h vindo do glad em third_party_generated/glad).
target_compile_definitions(imgui PUBLIC GLFW_INCLUDE_NONE)
# Loader explícito: sem isso o imgui tentaria auto-detectar via __has_include()
# e cairia certo mesmo (achou <glad/gl.h> => IMGUI_IMPL_OPENGL_LOADER_GLAD2),
# mas deixamos explícito pra não depender de detecção implícita do compilador.
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD2)

target_link_libraries(KizuriEngine
    PUBLIC
        glfw
        glad
        glm::glm
        EnTT::EnTT
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        imgui
        BulletDynamics
        BulletCollision
        LinearMath
        box2d
        ${CMAKE_DL_LIBS}
)

if(WIN32)
    # ole32: CoCreateInstance/CoInitializeEx/CoTaskMemFree (kizuri/core/FileDialog.cpp, IFileDialog).
    # MinGW linka user32/gdi32 por padrão pra alvo Windows, mas não ole32 — precisa ser explícito.
    target_link_libraries(KizuriEngine PUBLIC ole32)
endif()

target_compile_definitions(KizuriEngine PUBLIC
    KZ_PLATFORM_$<UPPER_CASE:$<PLATFORM_ID>>
    GLFW_INCLUDE_NONE
)

if(MSVC)
    target_compile_options(KizuriEngine PUBLIC /W4 /permissive-)
else()
    target_compile_options(KizuriEngine PUBLIC -Wall -Wextra -Wno-unused-parameter)
endif()

target_compile_definitions(KizuriEngine PUBLIC
    $<$<CONFIG:Debug>:KZ_DEBUG>
    $<$<CONFIG:Release>:KZ_RELEASE>
)
