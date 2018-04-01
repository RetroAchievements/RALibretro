#pragma once

#include "libretro/Components.h"

#include <SDL_opengl.h>

namespace Gl
{
	void init(libretro::LoggerComponent* logger);
  bool ok();

	void genTextures(GLsizei n, GLuint* textures);
	void deleteTextures(GLsizei n, const GLuint* textures);
	void bindTexture(GLenum target, GLuint texture);
  void activeTexture(GLenum texture);
	void texParameteri(GLenum target, GLenum pname, GLint param);
	void texImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data);
	void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels);
	void getTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid* pixels);

	void genBuffers(GLsizei n, GLuint* buffers);
	void deleteBuffers(GLsizei n, const GLuint* buffers);
	void bindBuffer(GLenum target, GLuint buffer);
	void bufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
	void vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
	void enableVertexAttribArray(GLuint index);

	GLuint createShader(GLenum shaderType);
	void deleteShader(GLuint shader);
	void shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
	void compileShader(GLuint shader);
	void getShaderiv(GLuint shader, GLenum pname, GLint* params);
	void getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
	GLuint createShader(GLenum shaderType, const char* source); // helper

	GLuint createProgram();
	void deleteProgram(GLuint program);
	void attachShader(GLuint program, GLuint shader);
	void linkProgram(GLuint program);
	void validateProgram(GLuint program);
	void getProgramiv(GLuint program, GLenum pname, GLint* params);
	void getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
	void useProgram(GLuint program);
	GLint getAttribLocation(GLuint program, const GLchar* name);
	GLint getUniformLocation(GLuint program, const GLchar* name);
  void uniform1i(GLint location, GLint v0);
	void uniform2f(GLint location, GLfloat v0, GLfloat v1);
	GLuint createProgram(const char* vertexShader, const char* fragmentShader); // helper

	void genFramebuffers(GLsizei n, GLuint* ids);
	void deleteFramebuffers(GLsizei n, GLuint* ids);
	void bindFramebuffer(GLenum target, GLuint framebuffer);
	void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	void framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
	void drawBuffers(GLsizei n, const GLenum* bufs);

	void genRenderbuffers(GLsizei n, GLuint* renderbuffers);
	void deleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
	void bindRenderbuffer(GLenum target, GLuint renderbuffer);
	void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

	void clearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	void clear(GLbitfield mask);
	void enable(GLenum cap);
	void disable(GLenum cap);
	void blendFunc(GLenum sfactor, GLenum dfactor);
	void viewport(GLint x, GLint y, GLsizei width, GLsizei height);
	void drawArrays(GLenum mode, GLint first, GLsizei count);
}
