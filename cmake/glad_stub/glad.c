#include "glad/glad.h"
#include <string.h>

void (KZGL_APIENTRY *glViewport)(GLint, GLint, GLsizei, GLsizei) = NULL;
void (KZGL_APIENTRY *glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
void (KZGL_APIENTRY *glClear)(GLbitfield) = NULL;
void (KZGL_APIENTRY *glEnable)(GLenum) = NULL;
void (KZGL_APIENTRY *glDisable)(GLenum) = NULL;
void (KZGL_APIENTRY *glBlendFunc)(GLenum, GLenum) = NULL;
void (KZGL_APIENTRY *glDepthFunc)(GLenum) = NULL;
void (KZGL_APIENTRY *glCullFace)(GLenum) = NULL;

void (KZGL_APIENTRY *glGenBuffers)(GLsizei, GLuint*) = NULL;
void (KZGL_APIENTRY *glBindBuffer)(GLenum, GLuint) = NULL;
void (KZGL_APIENTRY *glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = NULL;
void (KZGL_APIENTRY *glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) = NULL;
void (KZGL_APIENTRY *glDeleteBuffers)(GLsizei, const GLuint*) = NULL;

void (KZGL_APIENTRY *glGenVertexArrays)(GLsizei, GLuint*) = NULL;
void (KZGL_APIENTRY *glBindVertexArray)(GLuint) = NULL;
void (KZGL_APIENTRY *glDeleteVertexArrays)(GLsizei, const GLuint*) = NULL;
void (KZGL_APIENTRY *glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = NULL;
void (KZGL_APIENTRY *glEnableVertexAttribArray)(GLuint) = NULL;
void (KZGL_APIENTRY *glVertexAttribDivisor)(GLuint, GLuint) = NULL;

GLuint (KZGL_APIENTRY *glCreateShader)(GLenum) = NULL;
void (KZGL_APIENTRY *glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = NULL;
void (KZGL_APIENTRY *glCompileShader)(GLuint) = NULL;
void (KZGL_APIENTRY *glGetShaderiv)(GLuint, GLenum, GLint*) = NULL;
void (KZGL_APIENTRY *glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = NULL;
void (KZGL_APIENTRY *glDeleteShader)(GLuint) = NULL;

GLuint (KZGL_APIENTRY *glCreateProgram)(void) = NULL;
void (KZGL_APIENTRY *glAttachShader)(GLuint, GLuint) = NULL;
void (KZGL_APIENTRY *glLinkProgram)(GLuint) = NULL;
void (KZGL_APIENTRY *glGetProgramiv)(GLuint, GLenum, GLint*) = NULL;
void (KZGL_APIENTRY *glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = NULL;
void (KZGL_APIENTRY *glUseProgram)(GLuint) = NULL;
void (KZGL_APIENTRY *glDeleteProgram)(GLuint) = NULL;

GLint (KZGL_APIENTRY *glGetUniformLocation)(GLuint, const GLchar*) = NULL;
void (KZGL_APIENTRY *glUniform1i)(GLint, GLint) = NULL;
void (KZGL_APIENTRY *glUniform1f)(GLint, GLfloat) = NULL;
void (KZGL_APIENTRY *glUniform2f)(GLint, GLfloat, GLfloat) = NULL;
void (KZGL_APIENTRY *glUniform3f)(GLint, GLfloat, GLfloat, GLfloat) = NULL;
void (KZGL_APIENTRY *glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
void (KZGL_APIENTRY *glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = NULL;
void (KZGL_APIENTRY *glUniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*) = NULL;

void (KZGL_APIENTRY *glGenTextures)(GLsizei, GLuint*) = NULL;
void (KZGL_APIENTRY *glBindTexture)(GLenum, GLuint) = NULL;
void (KZGL_APIENTRY *glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = NULL;
void (KZGL_APIENTRY *glTexParameteri)(GLenum, GLenum, GLint) = NULL;
void (KZGL_APIENTRY *glTexParameterfv)(GLenum, GLenum, const GLfloat*) = NULL;
void (KZGL_APIENTRY *glGenerateMipmap)(GLenum) = NULL;
void (KZGL_APIENTRY *glActiveTexture)(GLenum) = NULL;
void (KZGL_APIENTRY *glDeleteTextures)(GLsizei, const GLuint*) = NULL;

void (KZGL_APIENTRY *glDrawArrays)(GLenum, GLint, GLsizei) = NULL;
void (KZGL_APIENTRY *glDrawElements)(GLenum, GLsizei, GLenum, const void*) = NULL;
void (KZGL_APIENTRY *glDrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei) = NULL;
void (KZGL_APIENTRY *glDrawElementsInstanced)(GLenum, GLsizei, GLenum, const void*, GLsizei) = NULL;

void (KZGL_APIENTRY *glGenFramebuffers)(GLsizei, GLuint*) = NULL;
void (KZGL_APIENTRY *glBindFramebuffer)(GLenum, GLuint) = NULL;
void (KZGL_APIENTRY *glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = NULL;
GLenum (KZGL_APIENTRY *glCheckFramebufferStatus)(GLenum) = NULL;
void (KZGL_APIENTRY *glDeleteFramebuffers)(GLsizei, const GLuint*) = NULL;
void (KZGL_APIENTRY *glDrawBuffer)(GLenum) = NULL;
void (KZGL_APIENTRY *glReadBuffer)(GLenum) = NULL;

const GLubyte* (KZGL_APIENTRY *glGetString)(GLenum) = NULL;
void (KZGL_APIENTRY *glGetIntegerv)(GLenum, GLint*) = NULL;

static void* kz_load(KZGLLoaderFn loader, const char* name) {
    return loader(name);
}

int gladLoadGL(KZGLLoaderFn loader) {
    if (!loader) return 0;

#define LOAD(name) *(void**)&name = kz_load(loader, #name); if (!name) return 0;

    LOAD(glViewport) LOAD(glClearColor) LOAD(glClear) LOAD(glEnable) LOAD(glDisable)
    LOAD(glBlendFunc) LOAD(glDepthFunc) LOAD(glCullFace)
    LOAD(glGenBuffers) LOAD(glBindBuffer) LOAD(glBufferData) LOAD(glBufferSubData) LOAD(glDeleteBuffers)
    LOAD(glGenVertexArrays) LOAD(glBindVertexArray) LOAD(glDeleteVertexArrays)
    LOAD(glVertexAttribPointer) LOAD(glEnableVertexAttribArray) LOAD(glVertexAttribDivisor)
    LOAD(glCreateShader) LOAD(glShaderSource) LOAD(glCompileShader) LOAD(glGetShaderiv)
    LOAD(glGetShaderInfoLog) LOAD(glDeleteShader)
    LOAD(glCreateProgram) LOAD(glAttachShader) LOAD(glLinkProgram) LOAD(glGetProgramiv)
    LOAD(glGetProgramInfoLog) LOAD(glUseProgram) LOAD(glDeleteProgram)
    LOAD(glGetUniformLocation) LOAD(glUniform1i) LOAD(glUniform1f) LOAD(glUniform2f)
    LOAD(glUniform3f) LOAD(glUniform4f) LOAD(glUniformMatrix4fv) LOAD(glUniformMatrix3fv)
    LOAD(glGenTextures) LOAD(glBindTexture) LOAD(glTexImage2D) LOAD(glTexParameteri)
    LOAD(glTexParameterfv)
    LOAD(glGenerateMipmap) LOAD(glActiveTexture) LOAD(glDeleteTextures)
    LOAD(glDrawArrays) LOAD(glDrawElements) LOAD(glDrawArraysInstanced) LOAD(glDrawElementsInstanced)
    LOAD(glGenFramebuffers) LOAD(glBindFramebuffer) LOAD(glFramebufferTexture2D)
    LOAD(glCheckFramebufferStatus) LOAD(glDeleteFramebuffers)
    LOAD(glDrawBuffer) LOAD(glReadBuffer)
    LOAD(glGetString) LOAD(glGetIntegerv)

#undef LOAD
    return 1;
}
