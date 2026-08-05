# Converte uma LISTA de arquivos binários num header C++ com um registro de
# recursos embutidos (sistema `kzres://`) — o mesmo princípio do
# EmbedResource.cmake das fontes, mas generalizado pra N arquivos e com uma
# tabela name->bytes indexável em runtime.
#
# Uso (via add_custom_command, não chamado diretamente pelo usuário):
#   cmake -DOUTPUT_FILE=<saida.hpp> -DMANIFEST_FILE=<arquivo.manifest> -P EmbedContent.cmake
#
# O manifest é um arquivo texto com uma entrada por linha no formato:
#   virtual_name<TAB>caminho_absoluto
# (linhas vazias/comentário '#' ignoradas). O manifest evita meta-caracteres
# de shell no -D do custom command (| e ; quebravam no Ninja).
#
# Cada arquivo vira um array `char[]` inicializado por LITERAIS DE STRING
# "\xNN" em blocos de 2KB — NÃO arrays numéricos 0xNN (o MSVC é lentíssimo e
# às vezes estoura em agregados com milhões de entradas; literais de string
# são varridos em tempo linear e cada bloco fica bem abaixo de qualquer limite
# de tamanho de literal). O tamanho exato vai explícito (sizeof não serve:
# string literal carrega um NUL a mais).
# Gera, em OUTPUT_FILE:
#   namespace kizuri::detail {
#     inline const char kRes_0[] = "...";  inline constexpr size_t kResSize_0 = N;
#     ...
#     struct EmbeddedFileEntry { const char* Name; const std::uint8_t* Data; std::size_t Size; };
#     inline const EmbeddedFileEntry kEmbeddedFiles[] = { ... };
#     inline constexpr std::size_t kEmbeddedFileCount = N;
#   }

if(NOT DEFINED OUTPUT_FILE OR NOT DEFINED MANIFEST_FILE)
    message(FATAL_ERROR "EmbedContent.cmake requer OUTPUT_FILE e MANIFEST_FILE")
endif()

file(READ "${MANIFEST_FILE}" MANIFEST)
string(REPLACE "\n" ";" LINES "${MANIFEST}")

set(OUTPUT "#pragma once\n#include <cstddef>\n#include <cstdint>\n\nnamespace kizuri {\nnamespace detail {\n\nstruct EmbeddedFileEntry {\n    const char* Name;\n    const std::uint8_t* Data;\n    std::size_t Size;\n};\n\n")

set(INDEX 0)
foreach(line IN LISTS LINES)
    string(STRIP "${line}" line)
    if(line STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()
    string(FIND "${line}" "\t" SEP)
    if(SEP EQUAL -1)
        message(FATAL_ERROR "Linha de manifest inválida (esperava 'nome<TAB>caminho'): ${line}")
    endif()
    string(SUBSTRING "${line}" 0 ${SEP} NAME)
    math(EXPR PATH_START "${SEP} + 1")
    string(SUBSTRING "${line}" ${PATH_START} -1 PATH)

    if(NOT EXISTS "${PATH}")
        message(FATAL_ERROR "Arquivo do manifest não existe: ${PATH}")
    endif()

    file(READ "${PATH}" HEX HEX)
    string(LENGTH "${HEX}" HEXLEN)
    math(EXPR FILESIZE "${HEXLEN} / 2")
    # Bloco de 2KB de dados = 4096 chars hex = 16384 chars escapados por
    # literal (bem abaixo do limite de literal de qualquer compilador).
    set(CHUNK_CHARS 4096)
    set(OFF 0)
    string(APPEND OUTPUT "inline const char kRes_${INDEX}[] =\n")
    while(OFF LESS HEXLEN)
        string(SUBSTRING "${HEX}" ${OFF} ${CHUNK_CHARS} SUB)
        string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "\\\\x\\1" SUB_ESC "${SUB}")
        string(APPEND OUTPUT "    \"${SUB_ESC}\"\n")
        math(EXPR OFF "${OFF} + ${CHUNK_CHARS}")
    endwhile()
    string(APPEND OUTPUT ";\ninline constexpr std::size_t kResSize_${INDEX} = ${FILESIZE};\n\n")
    math(EXPR INDEX "${INDEX} + 1")
endforeach()

string(APPEND OUTPUT "inline const EmbeddedFileEntry kEmbeddedFiles[] = {\n")
set(INDEX 0)
foreach(line IN LISTS LINES)
    string(STRIP "${line}" line)
    if(line STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()
    string(FIND "${line}" "\t" SEP)
    string(SUBSTRING "${line}" 0 ${SEP} NAME)
    string(APPEND OUTPUT "    { \"${NAME}\", reinterpret_cast<const std::uint8_t*>(kRes_${INDEX}), kResSize_${INDEX} },\n")
    math(EXPR INDEX "${INDEX} + 1")
endforeach()
string(APPEND OUTPUT "};\n\ninline constexpr std::size_t kEmbeddedFileCount = ${INDEX};\n\n} // namespace detail\n} // namespace kizuri\n")

file(WRITE "${OUTPUT_FILE}" "${OUTPUT}")
message(STATUS "EmbedContent.cmake: ${INDEX} arquivo(s) embutido(s) em ${OUTPUT_FILE}")
