cmake_minimum_required(VERSION 3.24)
project(KizuriEngine VERSION 0.38.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Algumas dependências de terceiros (ex: bullet3, box2d) ainda declaram
# cmake_minimum_required() com versões antigas, incompatíveis com CMake
# >= 4.0. Isso garante que elas configurem mesmo assim, sem exigir que o
# usuário passe a flag manualmente.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

option(KZ_BUILD_SANDBOX "Build the sandbox example game" ON)
option(KZ_BUILD_EDITOR  "Build the Kizuri Editor"        ON)
option(KZ_BUILD_GAME    "Build the standalone game exe"   ON)
option(KZ_BUILD_TESTS   "Build unit tests"                OFF)
# v2 ainda é só documentação (docs/v2/); fica OFF por padrão até existir
# código real em kizuri_v2/.
option(KZ_BUILD_V2      "Build the Kizuri Engine v2 foundation" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# bin/, não lib/: KizuriEngine volta a ser SHARED (ver engine/CMakeLists.txt)
# agora que a causa raiz do crash foi isolada e corrigida (fronteira de
# runtime C++ duplicado — ver comentário grande logo abaixo), então o
# .dll/.so precisa estar achável em tempo de execução pelos executáveis
# que o carregam. No Windows isso já acontece de graça (uma .dll conta como
# "runtime" pro CMake, não "library", e cai em bin/ automaticamente); no
# Linux/macOS, sem isso o .so ficaria em lib/, separado do executável, e
# precisaria de RPATH/LD_LIBRARY_PATH extra só pra rodar o editor recém-compilado.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# No Windows com MinGW/GCC: NÃO usar link estático completo do runtime
# C++ (-static / -static-libstdc++) em nenhum alvo daqui pra frente.
#
# Isso é o motivo real, concreto, de SHARED nunca ter aberto (confirmado
# pelo dono do projeto testando no hardware real): esse bloco aplicava
# -static-libgcc/-static-libstdc++/-static A TODO MUNDO, sem restrição de
# alvo — inclusive à KizuriEngine quando ela era SHARED. Resultado: a
# KizuriEngine.dll embutia SUA PRÓPRIA cópia inteira do runtime C++
# (heap/malloc, tabela de unwind de exceção, locale/iostream), e o
# KizuriEditor.exe embutia OUTRA cópia separada do mesmo runtime. Isso é
# incompatível de verdade — a engine devolve pro editor o tempo todo objeto
# C++ que atravessa essa fronteira (Ref<T> = std::shared_ptr, std::string,
# std::vector, exceção do nlohmann::json, etc.), e alocar num heap/runtime
# e desalocar ou lançar exceção através de outro heap/runtime diferente é
# comportamento indefinido — trava o processo na hora, sem exceção C++ pra
# capturar, sem tempo do log dar flush. É exatamente o "abre e fecha sem
# log nenhum" que foi observado.
#
# A correção de verdade (não é só reverter STATIC/SHARED de novo): manter
# UM runtime só, compartilhado dinamicamente, pra todo o processo — engine
# E executáveis linkam a MESMA libstdc++-6.dll/libgcc_s_seh-1.dll/
# libwinpthread-1.dll em runtime, então não existe fronteira nenhuma pra
# esse tipo de objeto quebrar. O problema original que o -static resolvia
# (rodar num PC sem MinGW instalado, sem essas DLLs no PATH) é resolvido
# aqui de outro jeito: kz_copy_mingw_runtime() (definida logo abaixo) copia
# essas 3 DLLs de dentro da instalação do compilador pra bin/ depois do
# build, então o executável final continua portátil sem precisar embutir
# nada estaticamente.
#
# IMPORTANTE pra quem for testar isso: essa mudança AINDA NÃO FOI TESTADA
# no hardware real (Winlator/Wine) — é a correção que o diagnóstico acima
# aponta como causa raiz, mas só vira fato depois de alguém rodar de
# verdade. Se ainda travar, o próximo passo é olhar KizuriEngine.log e, se
# nem isso aparecer, testar se as 3 DLLs realmente foram parar em bin/
# (kz_copy_mingw_runtime pode estar falhando em achar o diretório do
# compilador em alguns setups de MinGW/MSYS2 — nesse caso copiar manual e
# confirmar que resolve, antes de mexer em mais nada).
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_link_options(-Wl,--no-insert-timestamp)

    # Localiza as DLLs de runtime do MinGW a partir do diretório onde o
    # próprio compilador está instalado — funciona tanto pra MSYS2 quanto
    # pra distribuições standalone do MinGW-w64, que sempre colocam
    # g++.exe e as DLLs de runtime na mesma pasta bin/.
    get_filename_component(KZ_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    function(kz_copy_mingw_runtime target)
        foreach(dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(EXISTS "${KZ_MINGW_BIN_DIR}/${dll}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${KZ_MINGW_BIN_DIR}/${dll}"
                        "$<TARGET_FILE_DIR:${target}>/${dll}"
                    COMMENT "Copiando ${dll} do runtime MinGW pra junto de ${target}"
                )
            endif()
        endforeach()
    endfunction()
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---------------------------------------------------------------------------
# Dependências de terceiros (todas via FetchContent, versões fixas/pinned)
# ---------------------------------------------------------------------------

# GLFW - janelas/contexto/input
# Android: GLFW não tem suporte a Android (o backend de lá é EGL direto —
# Window.cpp tem seção própria) — nem é baixado.
if(NOT ANDROID)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    # Evita exigir wayland-scanner/wayland-protocols em runners de CI que não
    # têm esses pacotes instalados; X11 continua habilitado normalmente.
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()

# GLM - matemática
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1)

# glad NÃO é baixado via FetchContent: os bindings (GL 4.5 core, gerados pelo
# glad2 em gen.glad.sh) ficam comitados em third_party_generated/glad/. Ver
# third_party_generated/glad/README.md para como regerar quando precisar de
# uma extensão nova.

# EnTT - ECS
FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.2)

# spdlog - logging
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1)

# Bullet3 - física 3D
FetchContent_Declare(bullet3
    GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_TAG        3.25)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)

# box2d - física 2D
FetchContent_Declare(box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        v2.4.2)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)

# miniaudio - áudio (single header)
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21)

# stb (stb_image, stb_truetype, etc) - single headers
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master)

# tinyobjloader - importação de modelos .obj
FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13)

# cgltf - importação de modelos glTF/glb (single header, o padrão da
# indústria de assets 3D: meshes, hierarquia de nós, materiais PBR)
FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        v1.15)

# ImGui - UI de debug / editor
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.9-docking)

# ImGuizmo - gizmos de transformação (mover/rotacionar/escalar) no viewport
# do editor. Só o KizuriEditor depende disso (ver editor/CMakeLists.txt) —
# o runtime da engine não precisa de gizmo nenhum. Usa master porque o
# projeto não faz releases com tag estável com frequência — fixar numa tag
# antiga quebra o checkout quando ela some/muda.
FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        master)

# nlohmann/json - serialização de cenas/assets
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3)

# libcurl + miniz — SÓ desktop: o atualizador do editor (menu Ajuda >
# Verificar Atualizações) baixa e instala releases novas. O Android não
# compila nada disso (o updater é UI de editor).
if(NOT ANDROID)
    FetchContent_Declare(curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG        curl-8_11_0)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    if(WIN32)
        # Windows: TLS nativo (Schannel) — nada de openssl no CI.
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
        set(CURL_WINDOWS_SSPI ON CACHE BOOL "" FORCE)
    else()
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        GIT_TAG        3.0.2)
endif()

if(ANDROID)
    FetchContent_MakeAvailable(glm entt spdlog miniaudio stb tinyobjloader cgltf json)
else()
    FetchContent_MakeAvailable(glfw glm entt spdlog miniaudio stb tinyobjloader cgltf json)
    FetchContent_MakeAvailable(curl miniz)
    if(NOT TARGET miniz)
        add_library(miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
        target_include_directories(miniz PUBLIC ${miniz_SOURCE_DIR})
    endif()
endif()

# Bullet e Box2D são maiores; deixamos como submódulos opcionais de física
FetchContent_MakeAvailable(bullet3 box2d)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()

add_subdirectory(engine)

# Guarda a existência da pasta: a v2 ainda não tem código, então mesmo que
# alguém ligue KZ_BUILD_V2 antes de criar kizuri_v2/, o configure não quebra.
# Quando a pasta existir, o subdiretório entra no build automaticamente.
if(KZ_BUILD_V2 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kizuri_v2")
    add_subdirectory(kizuri_v2)
endif()

# Android: o editor e o sandbox (ambos GLFW/ImGui desktop) não compilam —
# força OFF a menos que alguém peça explicitamente (vai quebrar de qualquer
# jeito; melhor um erro claro no configure).
if(ANDROID)
    if(KZ_BUILD_EDITOR)
        message(WARNING "KZ_BUILD_EDITOR não é suportado em Android — desligado.")
        set(KZ_BUILD_EDITOR OFF)
    endif()
    if(KZ_BUILD_SANDBOX)
        message(WARNING "KZ_BUILD_SANDBOX não é suportado em Android — desligado.")
        set(KZ_BUILD_SANDBOX OFF)
    endif()
endif()

if(KZ_BUILD_SANDBOX)
    add_subdirectory(examples/sandbox)
endif()

if(KZ_BUILD_GAME)
    add_subdirectory(examples/KizuriGame)
endif()

if(KZ_BUILD_EDITOR)
    add_subdirectory(editor)
endif()
