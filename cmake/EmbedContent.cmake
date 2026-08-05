# Converte uma LISTA de arquivos binários num header C++ com um registro de
# recursos embutidos (sistema `kzres://`) — o mesmo princípio do
# EmbedResource.cmake das fontes, mas generalizado pra N arquivos e com uma
# tabela name->bytes indexável em runtime.
#
# Uso (via add_custom_command, não chamado diretamente pelo usuário):
#   cmake -DOUTPUT_FILE=<saida.hpp> -DENTRIES="models/Cube.glb|<abs path 1>;skies/x.hdr|<abs path 2>" -P EmbedContent.cmake
#
# Cada entry é "virtual_name|caminho_absoluto". Gera, em OUTPUT_FILE:
#   namespace kizuri::detail {
#     struct EmbeddedFileEntry { const char* Name; const std::uint8_t* Data; std::size_t Size; };
#     inline const EmbeddedFileEntry kEmbeddedFiles[] = { ... };
#     inline constexpr std::size_t kEmbeddedFileCount = N;
#   }

if(NOT DEFINED OUTPUT_FILE OR NOT DEFINED ENTRIES)
    message(FATAL_ERROR "EmbedContent.cmake requer OUTPUT_FILE e ENTRIES")
endif()

set(OUTPUT "#pragma once\n#include <cstddef>\n#include <cstdint>\n\nnamespace kizuri {\nnamespace detail {\n\nstruct EmbeddedFileEntry {\n    const char* Name;\n    const std::uint8_t* Data;\n    std::size_t Size;\n};\n\n")

set(INDEX 0)
foreach(entry IN LISTS ENTRIES)
    string(FIND "${entry}" "|" SEP)
    if(SEP EQUAL -1)
        message(FATAL_ERROR "Entry inválida (esperava 'nome|caminho'): ${entry}")
    endif()
    string(SUBSTRING "${entry}" 0 ${SEP} NAME)
    math(EXPR PATH_START "${SEP} + 1")
    string(SUBSTRING "${entry}" ${PATH_START} -1 PATH)

    file(READ "${PATH}" HEX_CONTENT HEX)
    string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," BYTES_LIST "${HEX_CONTENT}")

    string(APPEND OUTPUT "inline const std::uint8_t kRes_${INDEX}[] = { ${BYTES_LIST} };\n\n")
    math(EXPR INDEX "${INDEX} + 1")
endforeach()

string(APPEND OUTPUT "inline const EmbeddedFileEntry kEmbeddedFiles[] = {\n")
set(INDEX 0)
foreach(entry IN LISTS ENTRIES)
    string(FIND "${entry}" "|" SEP)
    string(SUBSTRING "${entry}" 0 ${SEP} NAME)
    string(APPEND OUTPUT "    { \"${NAME}\", kRes_${INDEX}, sizeof(kRes_${INDEX}) },\n")
    math(EXPR INDEX "${INDEX} + 1")
endforeach()
string(APPEND OUTPUT "};\n\ninline constexpr std::size_t kEmbeddedFileCount = ${INDEX};\n\n} // namespace detail\n} // namespace kizuri\n")

file(WRITE "${OUTPUT_FILE}" "${OUTPUT}")
message(STATUS "EmbedContent.cmake: ${INDEX} arquivo(s) embutido(s) em ${OUTPUT_FILE}")
