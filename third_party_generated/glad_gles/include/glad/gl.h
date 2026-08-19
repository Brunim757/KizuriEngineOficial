#pragma once

#include "glad/glad.h"
typedef GLADloadproc GLADloadfunc;
#define gladLoadGL(load) gladLoadGLES2Loader((GLADloadfunc)(load))