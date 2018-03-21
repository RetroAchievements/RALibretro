#include "Gl.h"

#include <SDL_video.h>

Gl::Gl(libretro::LoggerComponent* logger): _logger(logger), _ok(true)
{
  glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers");
  glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)SDL_GL_GetProcAddress("glBindRenderbuffer");
  glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
  glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D");
  glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)SDL_GL_GetProcAddress("glRenderbufferStorage");
  glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)SDL_GL_GetProcAddress("glFramebufferRenderbuffer");
}

void Gl::genFramebuffers(GLsizei n, GLuint* ids)
{
  glGenFramebuffers(n, ids);
  check();
}

void Gl::genTextures(GLsizei n, GLuint* textures)
{
  glGenTextures(n, textures);
  check();
}

void Gl::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
  glBindRenderbuffer(target, renderbuffer);
  check();
}

void Gl::bindFramebuffer(GLenum target, GLuint framebuffer)
{
  glBindFramebuffer(target, framebuffer);
  check();
}

void Gl::bindTexture(GLenum target, GLuint texture)
{
  glBindTexture(target, texture);
  check();
}

void Gl::texImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data)
{
  glTexImage2D(target, level, internalFormat, width, height, border, format, type, data);
  check();

}
void Gl::texParameteri(GLenum target, GLenum pname, GLint param)
{
  glTexParameteri(target, pname, param);
  check();
}

void Gl::framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  glFramebufferTexture2D(target, attachment, textarget, texture, level);
  check();
}

void Gl::renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
  glRenderbufferStorage(target, internalformat, width, height);
  check();
}

void Gl::framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
  glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
  check();
}

void Gl::enable(GLenum cap)
{
  glEnable(cap);
  check();
}

void Gl::disable(GLenum cap)
{
  glDisable(cap);
  check();
}

void Gl::blendFunc(GLenum sfactor, GLenum dfactor)
{
  glBlendFunc(sfactor, dfactor);
  check();
}

void Gl::check()
{
  GLenum err = glGetError();

  if (err != GL_NO_ERROR)
  {
    _ok = false;
    
    do
    {
      switch (err)
      {
      case GL_INVALID_OPERATION:
        _logger->printf(RETRO_LOG_ERROR, "INVALID_OPERATION");
        break;

      case GL_INVALID_ENUM:
        _logger->printf(RETRO_LOG_ERROR, "INVALID_ENUM");
        break;

      case GL_INVALID_VALUE:
        _logger->printf(RETRO_LOG_ERROR, "INVALID_VALUE");
        break;

      case GL_OUT_OF_MEMORY:
        _logger->printf(RETRO_LOG_ERROR, "OUT_OF_MEMORY");
        break;

      case GL_INVALID_FRAMEBUFFER_OPERATION:
        _logger->printf(RETRO_LOG_ERROR, "INVALID_FRAMEBUFFER_OPERATION");
        break;
      }

      err = glGetError();
    }
    while (err != GL_NO_ERROR);
  }
}
