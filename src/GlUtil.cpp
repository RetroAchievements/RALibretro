#include "GlUtil.h"

#include "Gl.h"

#define TAG "[GLU] "

static libretro::LoggerComponent* s_logger;

void GlUtil::init(libretro::LoggerComponent* logger)
{
  s_logger = logger;
}

GLuint GlUtil::createTexture(GLsizei width, GLsizei height, GLint internalFormat, GLenum format, GLenum type, GLenum filter)
{
  if (!Gl::ok()) return 0;

  GLuint texture;
  Gl::genTextures(1, &texture);
  Gl::bindTexture(GL_TEXTURE_2D, texture);

  Gl::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  Gl::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

  Gl::texImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, NULL);

  if (!Gl::ok())
  {
    Gl::deleteTextures(1, &texture);
    return 0;
  }

  return texture;
}

GLuint GlUtil::createShader(GLenum shaderType, const char* source)
{
  if (!Gl::ok()) return 0;

  GLuint shader = Gl::createShader(shaderType);
  Gl::shaderSource(shader, 1, &source, NULL);
  Gl::compileShader(shader);

  GLint status;
  Gl::getShaderiv(shader, GL_COMPILE_STATUS, &status);

  if (status == GL_FALSE)
  {
    char buffer[4096];
    Gl::getShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
    s_logger->error(TAG "Error in shader: %s", buffer);
    Gl::deleteShader(shader);
    return 0;
  }

  if (!Gl::ok())
  {
    Gl::deleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint GlUtil::createProgram(const char* vertexShader, const char* fragmentShader)
{
  if (!Gl::ok()) return 0;

  GLuint vs = createShader(GL_VERTEX_SHADER, vertexShader);
  GLuint fs = createShader(GL_FRAGMENT_SHADER, fragmentShader);
  GLuint program = Gl::createProgram();

  Gl::attachShader(program, vs);
  Gl::attachShader(program, fs);
  Gl::linkProgram(program);

  Gl::deleteShader(vs);
  Gl::deleteShader(fs);

  Gl::validateProgram(program);

  GLint status;
  Gl::getProgramiv(program, GL_LINK_STATUS, &status);

  if (status == GL_FALSE)
  {
    char buffer[4096];
    Gl::getProgramInfoLog(program, sizeof(buffer), NULL, buffer);
    s_logger->error(TAG "Error in shader program: %s", buffer);
    Gl::deleteProgram(program);
    return 0;
  }

  if (!Gl::ok())
  {
    Gl::deleteProgram(program);
    return 0;
  }

  return program;
}

GLuint GlUtil::createFramebuffer(GLuint* renderbuffer, GLsizei width, GLsizei height, GLuint texture, bool depth, bool stencil)
{
  if (!Gl::ok()) return 0;

  GLuint framebuffer;
  Gl::genFramebuffers(1, &framebuffer);
  Gl::bindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  Gl::framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

  *renderbuffer = 0;

  if (depth && stencil)
  {
    Gl::genRenderbuffers(1, renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);
    Gl::renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    Gl::framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, 0);
  }
  else if (depth)
  {
    Gl::genRenderbuffers(1, renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);
    Gl::renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    Gl::framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  if (Gl::checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    if (renderbuffer != 0)
    {
      Gl::deleteRenderbuffers(1, renderbuffer);
    }

    Gl::deleteFramebuffers(1, &framebuffer);
    *renderbuffer = 0;
    return 0;
  }

  Gl::clearColor(0, 0, 0, 1);
  Gl::clear(GL_COLOR_BUFFER_BIT);

  Gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
  return framebuffer;
}
