#pragma once

#include "libretro/Components.h"

#include "Gl.h"

namespace GlUtil
{
  void init(libretro::LoggerComponent* logger);

  GLuint createTexture(GLsizei width, GLsizei height, GLint internalFormat, GLenum format, GLenum type, GLenum filter);
  GLuint createShader(GLenum shaderType, const char* source);
  GLuint createProgram(const char* vertexShader, const char* fragmentShader);
  GLuint createFramebuffer(GLuint *renderbuffer, GLsizei width, GLsizei height, GLuint texture, bool depth, bool stencil);
}
