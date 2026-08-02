# -----------------------------------------------------------------------------
# StageSDK.cmake — monta a pasta bin/sdk usada pelo "Compilar GameModule" do
# editor (GameModuleBuilder). O SDK é entregável: headers da engine + headers
# de terceiros que a API pública expõe + a lib de import da engine. Com ele o
# editor compila Source/*.cpp com NENHUM checkout do código-fonte e NENHUMA
# chamada a cmake em runtime — só o compilador instalado + alguns -I/-L.
#
# Parâmetros (passados via -D ao rodar este script):
#   KZ_SDK_DIR           onde montar (ex: build/bin/sdk)
#   KZ_ENGINE_INCLUDE    engine/include (do próprio repo)
#   KZ_GLM_SRC / KZ_ENTT_SRC / KZ_SPDLOG_SRC / KZ_JSON_SRC
#                        diretórios-fonte dos FetchContent de varies
#   KZ_IMPORT_LIB_DIR    diretório de artifactos tipo CMAKE_ARCHIVE_OUTPUT_DIRECTORY
# -----------------------------------------------------------------------------

if(NOT DEFINED KZ_SDK_DIR)
    message(FATAL_ERROR "StageSDK: KZ_SDK_DIR obrigatório")
endif()

file(MAKE_DIRECTORY "${KZ_SDK_DIR}/include")
file(MAKE_DIRECTORY "${KZ_SDK_DIR}/lib")

# ---- headers da própria engine (Kizuri.hpp + kizuri/...) --------------------
# file(COPY dir DESTINATION dest) cria dest/<nome-do-dir>. A pasta de origem
# já se chama "include", então o destino é a RAIZ do SDK — o resultado final
# fica <sdk>/include/kizuri/... e <sdk>/include/Kizuri.hpp.
file(COPY "${KZ_ENGINE_INCLUDE}" DESTINATION "${KZ_SDK_DIR}")

# ---- tercéiros que a API pública puxa (#include <glm/*>, <entt/entt.hpp>,
# <spdlog/spdlog.h>, <nlohmann/json.hpp>) ------------------------------------
if(EXISTS "${KZ_GLM_SRC}/glm/glm.hpp")
    file(COPY "${KZ_GLM_SRC}/glm" DESTINATION "${KZ_SDK_DIR}/include")
elseif(EXISTS "${KZ_GLM_SRC}/glm.hpp")
    file(COPY "${KZ_GLM_SRC}" DESTINATION "${KZ_SDK_DIR}/include")
endif()

if(EXISTS "${KZ_ENTT_SRC}/src/entt/entt.hpp")
    file(COPY "${KZ_ENTT_SRC}/src/entt" DESTINATION "${KZ_SDK_DIR}/include")
elseif(EXISTS "${KZ_ENTT_SRC}/src/entt.hpp")
    file(COPY "${KZ_ENTT_SRC}/src" DESTINATION "${KZ_SDK_DIR}/include")
endif()

if(EXISTS "${KZ_SPDLOG_SRC}/include/spdlog/spdlog.h")
    file(COPY "${KZ_SPDLOG_SRC}/include/spdlog" DESTINATION "${KZ_SDK_DIR}/include")
endif()

if(EXISTS "${KZ_JSON_SRC}/include/nlohmann/json.hpp")
    file(COPY "${KZ_JSON_SRC}/include/nlohmann" DESTINATION "${KZ_SDK_DIR}/include")
endif()

# ---- lib de import da engine -------------------------------------------------
# Windows/MinGW e MSVC geram lib de import em CMAKE_ARCHIVE_OUTPUT_DIRECTORY
# (build/lib). Linux não gera .lib; o link mira o .so que já está em bin/.
if(DEFINED KZ_IMPORT_LIB_DIR AND EXISTS "${KZ_IMPORT_LIB_DIR}")
    file(GLOB _import_libs LIST_DIRECTORIES false
        "${KZ_IMPORT_LIB_DIR}/libKizuriEngine.dll.a"
        "${KZ_IMPORT_LIB_DIR}/KizuriEngine.lib")
    foreach(lib IN LISTS _import_libs)
        file(COPY "${lib}" DESTINATION "${KZ_SDK_DIR}/lib")
    endforeach()
endif()