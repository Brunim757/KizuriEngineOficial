#pragma once
// Shim Android: a engine inclui <glad/gl.h> em todos os renderers. No
// Android o loader é o glad gerado pra GLES (glad_gles/include/glad/glad.h),
// que expõe as MESMAS funções com os MESMOS nomes (GLES 3.x compartilha o
// núcleo com GL 3.3). Só a função de load e o tipo do callback mudam.
#include "glad/glad.h"
#define gladLoadGL(load) gladLoadGLES2Loader((GLADloadproc)(load))