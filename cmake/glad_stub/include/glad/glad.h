




#ifndef KIZURI_GLAD_H
#define KIZURI_GLAD_H

#include <KHR/khrplatform.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define KZGL_APIENTRY __stdcall
#else
  #define KZGL_APIENTRY
#endif

typedef void (*GLADloadproc)(const char* name);
typedef void* (*KZGLLoaderFn)(const char* name);

int gladLoadGL(KZGLLoaderFn loader);


typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef double GLdouble;
typedef char GLchar;
typedef khronos_ssize_t GLsizeiptr;
typedef khronos_intptr_t GLintptr;


#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_LINES 0x0001
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_INT 0x1405
#define GL_UNSIGNED_BYTE 0x1401
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_NEAREST 0x2600
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_RGBA8 0x8058
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_STENCIL 0x84F9
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_CULL_FACE 0x0B44
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_LEQUAL 0x0203
#define GL_MULTISAMPLE 0x809D
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_NONE 0
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_VIEWPORT 0x0BA2


extern void (KZGL_APIENTRY *glViewport)(GLint x, GLint y, GLsizei w, GLsizei h);
extern void (KZGL_APIENTRY *glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
extern void (KZGL_APIENTRY *glClear)(GLbitfield mask);
extern void (KZGL_APIENTRY *glEnable)(GLenum cap);
extern void (KZGL_APIENTRY *glDisable)(GLenum cap);
extern void (KZGL_APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
extern void (KZGL_APIENTRY *glDepthFunc)(GLenum func);
extern void (KZGL_APIENTRY *glCullFace)(GLenum mode);

extern void (KZGL_APIENTRY *glGenBuffers)(GLsizei n, GLuint* buffers);
extern void (KZGL_APIENTRY *glBindBuffer)(GLenum target, GLuint buffer);
extern void (KZGL_APIENTRY *glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
extern void (KZGL_APIENTRY *glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
extern void (KZGL_APIENTRY *glDeleteBuffers)(GLsizei n, const GLuint* buffers);

extern void (KZGL_APIENTRY *glGenVertexArrays)(GLsizei n, GLuint* arrays);
extern void (KZGL_APIENTRY *glBindVertexArray)(GLuint array);
extern void (KZGL_APIENTRY *glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
extern void (KZGL_APIENTRY *glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
extern void (KZGL_APIENTRY *glEnableVertexAttribArray)(GLuint index);
extern void (KZGL_APIENTRY *glVertexAttribDivisor)(GLuint index, GLuint divisor);

extern GLuint (KZGL_APIENTRY *glCreateShader)(GLenum type);
extern void (KZGL_APIENTRY *glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
extern void (KZGL_APIENTRY *glCompileShader)(GLuint shader);
extern void (KZGL_APIENTRY *glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
extern void (KZGL_APIENTRY *glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void (KZGL_APIENTRY *glDeleteShader)(GLuint shader);

extern GLuint (KZGL_APIENTRY *glCreateProgram)(void);
extern void (KZGL_APIENTRY *glAttachShader)(GLuint program, GLuint shader);
extern void (KZGL_APIENTRY *glLinkProgram)(GLuint program);
extern void (KZGL_APIENTRY *glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
extern void (KZGL_APIENTRY *glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void (KZGL_APIENTRY *glUseProgram)(GLuint program);
extern void (KZGL_APIENTRY *glDeleteProgram)(GLuint program);

extern GLint (KZGL_APIENTRY *glGetUniformLocation)(GLuint program, const GLchar* name);
extern void (KZGL_APIENTRY *glUniform1i)(GLint location, GLint v0);
extern void (KZGL_APIENTRY *glUniform1f)(GLint location, GLfloat v0);
extern void (KZGL_APIENTRY *glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void (KZGL_APIENTRY *glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (KZGL_APIENTRY *glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (KZGL_APIENTRY *glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
extern void (KZGL_APIENTRY *glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

extern void (KZGL_APIENTRY *glGenTextures)(GLsizei n, GLuint* textures);
extern void (KZGL_APIENTRY *glBindTexture)(GLenum target, GLuint texture);
extern void (KZGL_APIENTRY *glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
extern void (KZGL_APIENTRY *glTexParameteri)(GLenum target, GLenum pname, GLint param);
extern void (KZGL_APIENTRY *glTexParameterfv)(GLenum target, GLenum pname, const GLfloat* params);
extern void (KZGL_APIENTRY *glGenerateMipmap)(GLenum target);
extern void (KZGL_APIENTRY *glActiveTexture)(GLenum texture);
extern void (KZGL_APIENTRY *glDeleteTextures)(GLsizei n, const GLuint* textures);

extern void (KZGL_APIENTRY *glDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern void (KZGL_APIENTRY *glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
extern void (KZGL_APIENTRY *glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
extern void (KZGL_APIENTRY *glDrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);

extern void (KZGL_APIENTRY *glGenFramebuffers)(GLsizei n, GLuint* framebuffers);
extern void (KZGL_APIENTRY *glBindFramebuffer)(GLenum target, GLuint framebuffer);
extern void (KZGL_APIENTRY *glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern GLenum (KZGL_APIENTRY *glCheckFramebufferStatus)(GLenum target);
extern void (KZGL_APIENTRY *glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
extern void (KZGL_APIENTRY *glDrawBuffer)(GLenum buf);
extern void (KZGL_APIENTRY *glReadBuffer)(GLenum src);

extern const GLubyte* (KZGL_APIENTRY *glGetString)(GLenum name);
extern void (KZGL_APIENTRY *glGetIntegerv)(GLenum pname, GLint* data);

#ifdef __cplusplus
}
#endif
#endif 
