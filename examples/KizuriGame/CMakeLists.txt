# KizuriGame — o executável final de jogo. É o que transforma um projeto
# (cena .kzscene + GameModule com os scripts C++) num jogo que roda sozinho,
# sem o editor. Uso:
#
#   KizuriGame <cena.kzscene> [GameModule.ext]
#
# Sem argumentos, tenta "Start.kzscene" na pasta atual. Os caminhos de
# assets dentro da cena são relativos ao diretório de trabalho, então rode
# a partir da pasta do projeto/do jogo.
add_executable(KizuriGame src/GameApp.cpp)
target_link_libraries(KizuriGame PRIVATE KizuriEngine)
target_include_directories(KizuriGame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties(KizuriGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND COMMAND kz_copy_mingw_runtime)
    kz_copy_mingw_runtime(KizuriGame)
endif()

# No Windows, esconde o console em builds Release (comportamento de produto
# final "limpo" — igual ao Sandbox e ao Editor; mensagens vão pro log).
if(WIN32)
    target_link_options(KizuriGame PRIVATE $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-mwindows>)
    set_target_properties(KizuriGame PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()
