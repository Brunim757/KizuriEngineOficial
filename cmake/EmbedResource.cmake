# Converte um arquivo binário arbitrário num header C++ contendo os bytes
# como um array `const unsigned char[]`, pra que o recurso seja compilado
# direto no executável em vez de precisar ser distribuído como um arquivo
# solto ao lado dele (ex: fontes .ttf).
#
# Uso (via add_custom_command, não chamado diretamente pelo usuário):
#   cmake -DINPUT_FILE=<caminho.ttf> -DOUTPUT_FILE=<saida.hpp> -DVAR_NAME=<nome> -P EmbedResource.cmake
#
# Gera, em OUTPUT_FILE:
#   namespace kizuri::embedded { inline const unsigned char <VAR_NAME>[] = {...};
#                                 inline const unsigned long <VAR_NAME>_size = N; }

if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE OR NOT DEFINED VAR_NAME)
    message(FATAL_ERROR "EmbedResource.cmake requer INPUT_FILE, OUTPUT_FILE e VAR_NAME definidos")
endif()

file(READ "${INPUT_FILE}" HEX_CONTENT HEX)
string(LENGTH "${HEX_CONTENT}" HEX_LENGTH)
math(EXPR BYTE_COUNT "${HEX_LENGTH} / 2")

# Insere "0x" antes de cada par de dígitos hex e uma vírgula depois, pra
# transformar a string hex contínua numa lista de literais C++ (ex:
# "4a4554" vira "0x4a,0x45,0x54,").
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," BYTES_LIST "${HEX_CONTENT}")

file(WRITE "${OUTPUT_FILE}"
"// Gerado automaticamente por cmake/EmbedResource.cmake a partir de:
//   ${INPUT_FILE}
// Não edite manualmente — regenerado a cada build. Ver EmbedResource.cmake.
#pragma once

namespace kizuri::embedded {

inline const unsigned char ${VAR_NAME}[] = { ${BYTES_LIST} };
inline const unsigned long ${VAR_NAME}_size = ${BYTE_COUNT}UL;

} // namespace kizuri::embedded
")
