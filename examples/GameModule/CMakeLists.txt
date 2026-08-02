cmake_minimum_required(VERSION 3.20)
project(GameModule CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Algumas dependências da engine (box2d, bullet3) declaram
# cmake_minimum_required() antigas, incompatíveis com CMake >= 4.0. O mesmo
# guarda que o CMakeLists raiz usa — sem isso o configure quebra em CMake 4.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# ---------------------------------------------------------------------------
# Template de GameModule — a biblioteca dinâmica (.dll/.so) com o código C++
# do jogo que o editor carrega em Arquivo > Carregar GameModule.
#
# Funciona de dois jeitos:
#   1. Dentro do build da engine (o CMakeLists.txt raiz já adiciona este
#      diretório) — o alvo KizuriEngine já existe, nada mais a fazer.
#   2. Sozinho, copiando esta pasta pra dentro de um projeto — aponte pro
#      checkout da engine com:
#        cmake -B build -DKIZURI_ENGINE_DIR=/caminho/pra/Kizuri-Engine-main
# ---------------------------------------------------------------------------
if(NOT TARGET KizuriEngine)
    if(NOT DEFINED KIZURI_ENGINE_DIR OR KIZURI_ENGINE_DIR STREQUAL "")
        message(FATAL_ERROR "Defina -DKIZURI_ENGINE_DIR=/caminho/pra/Kizuri-Engine-main ao configurar este projeto.")
    endif()
    # add_subdirectory da RAIZ do repo (não só de engine/): as dependências
    # (FetchContent) e o glad são declarados no CMakeLists da raiz — o
    # subdiretório engine/ sozinho não configuraria. Os exemplos/editor
    # ficam desligados; o que importa aqui é só a KizuriEngine + este módulo.
    #
    # KizuriEngine é SHARED de propósito pra este caso: quando o editor já
    # rodando carrega este GameModule, o sistema operacional reconhece que
    # uma biblioteca chamada "KizuriEngine" já está carregada no processo e
    # reaproveita ELA — log, estado e alocação compartilhados entre editor e
    # script. Pra isso funcionar, compile com o MESMO compilador que compilou
    # o editor (mesma versão de MSVC/GCC/Clang) — nomes de símbolo e layout
    # de struct precisam bater.
    set(KZ_BUILD_SANDBOX    OFF CACHE BOOL "" FORCE)
    set(KZ_BUILD_EDITOR     OFF CACHE BOOL "" FORCE)
    set(KZ_BUILD_GAME       OFF CACHE BOOL "" FORCE)
    set(KZ_BUILD_GAMEMODULE OFF CACHE BOOL "" FORCE) # evita recursão: nós somos o GameModule
    add_subdirectory(${KIZURI_ENGINE_DIR} ${CMAKE_BINARY_DIR}/_kizuri_root)
endif()

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(GameModule SHARED ${GAME_SOURCES})
target_link_libraries(GameModule PRIVATE KizuriEngine)
target_include_directories(GameModule PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# O módulo depende da KizuriEngine.dll/.so em runtime — vai pra bin/ junto
# do editor e da engine pra carga pelo menu Carregar GameModule achar tudo.
# No Windows a engine já carregada é reaproveitada; no Linux o $ORIGIN faz o
# .so do módulo procurar a libKizuriEngine.so ao lado dele.
set_target_properties(GameModule PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
target_link_options(GameModule PRIVATE "$<$<PLATFORM_ID:Linux>:-Wl,-rpath,\\$ORIGIN>")

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND COMMAND kz_copy_mingw_runtime)
    kz_copy_mingw_runtime(GameModule)
endif()
