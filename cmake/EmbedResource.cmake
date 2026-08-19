
if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE OR NOT DEFINED VAR_NAME)
    message(FATAL_ERROR "EmbedResource.cmake requer INPUT_FILE, OUTPUT_FILE e VAR_NAME definidos")
endif()

file(READ "${INPUT_FILE}" HEX_CONTENT HEX)
string(LENGTH "${HEX_CONTENT}" HEX_LENGTH)
math(EXPR BYTE_COUNT "${HEX_LENGTH} / 2")

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
