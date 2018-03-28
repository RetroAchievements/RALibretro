#include "Gl.h"

#include <SDL_video.h>
#include <SDL_opengl_glext.h>

bool Gl::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  _ok = true;
  
  _glGenBuffers = (PFNGLGENBUFFERSPROC)getProcAddress("glGenBuffers");
  _glBindBuffer = (PFNGLBINDBUFFERPROC)getProcAddress("glBindBuffer");
  _glBufferData = (PFNGLBUFFERDATAPROC)getProcAddress("glBufferData");
  _glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)getProcAddress("glVertexAttribPointer");
  _glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)getProcAddress("glEnableVertexAttribArray");

  _glCreateShader = (PFNGLCREATESHADERPROC)getProcAddress("glCreateShader");
  _glDeleteShader = (PFNGLDELETESHADERPROC)getProcAddress("glDeleteShader");
  _glShaderSource = (PFNGLSHADERSOURCEPROC)getProcAddress("glShaderSource");
  _glCompileShader = (PFNGLCOMPILESHADERPROC)getProcAddress("glCompileShader");
  _glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)getProcAddress("glGetShaderInfoLog");

  _glCreateProgram = (PFNGLCREATEPROGRAMPROC)getProcAddress("glCreateProgram");
  _glDeleteProgram = (PFNGLDELETEPROGRAMPROC)getProcAddress("glDeleteProgram");
  _glAttachShader = (PFNGLATTACHSHADERPROC)getProcAddress("glAttachShader");
  _glLinkProgram = (PFNGLLINKPROGRAMPROC)getProcAddress("glLinkProgram");
  _glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)getProcAddress("glValidateProgram");
  _glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)getProcAddress("glGetProgramInfoLog");
  _glUseProgram = (PFNGLUSEPROGRAMPROC)getProcAddress("glUseProgram");
  _glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)getProcAddress("glGetAttribLocation");
  _glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)getProcAddress("glGetUniformLocation");
  _glUniform2f = (PFNGLUNIFORM2FPROC)getProcAddress("glUniform2f");

  _glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)getProcAddress("glGenFramebuffers");
  _glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)getProcAddress("glDeleteFramebuffers");
  _glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)getProcAddress("glBindRenderbuffer");
  _glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)getProcAddress("glBindFramebuffer");
  _glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)getProcAddress("glFramebufferTexture2D");
  _glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)getProcAddress("glRenderbufferStorage");
  _glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)getProcAddress("glFramebufferRenderbuffer");
  _glDrawBuffers = (PFNGLDRAWBUFFERSPROC)getProcAddress("glDrawBuffers");

  _glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)getProcAddress("glGenRenderbuffers");
  _glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)getProcAddress("glDeleteRenderbuffers");

  return _ok;
}

void Gl::genTextures(GLsizei n, GLuint* textures)
{
  if (!_ok) return;
  glGenTextures(n, textures);
  check(__FUNCTION__);
}

void Gl::deleteTextures(GLsizei n, const GLuint* textures)
{
  if (!_ok) return;
  glDeleteTextures(n, textures);
  check(__FUNCTION__);
}

void Gl::bindTexture(GLenum target, GLuint texture)
{
  if (!_ok) return;
  glBindTexture(target, texture);
  check(__FUNCTION__);
}

void Gl::texParameteri(GLenum target, GLenum pname, GLint param)
{
  if (!_ok) return;
  glTexParameteri(target, pname, param);
  check(__FUNCTION__);
}

void Gl::texImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data)
{
  if (!_ok) return;
  glTexImage2D(target, level, internalFormat, width, height, border, format, type, data);
  check(__FUNCTION__);
}

void Gl::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels)
{
  if (!_ok) return;
  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
  check(__FUNCTION__);
}

void Gl::genBuffers(GLsizei n, GLuint* buffers)
{
  if (!_ok) return;
  _glGenBuffers(n, buffers);
  check(__FUNCTION__);
}

void Gl::deleteBuffers(GLsizei n, const GLuint* buffers)
{
  if (!_ok) return;
  _glDeleteBuffers(n, buffers);
  check(__FUNCTION__);
}

void Gl::bindBuffer(GLenum target, GLuint buffer)
{
  if (!_ok) return;
  _glBindBuffer(target, buffer);
  check(__FUNCTION__);
}

void Gl::bufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
  if (!_ok) return;
  _glBufferData(target, size, data, usage);
  check(__FUNCTION__);
}

void Gl::vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer)
{
  if (!_ok) return;
  _glVertexAttribPointer(index, size, type, normalized, stride, pointer);
  check(__FUNCTION__);
}

void Gl::enableVertexAttribArray(GLuint index)
{
  if (!_ok) return;
  _glEnableVertexAttribArray(index);
  check(__FUNCTION__);
}

GLuint Gl::createShader(GLenum shaderType)
{
  if (!_ok) return 0;
  GLuint shader = _glCreateShader(shaderType);
  check(__FUNCTION__, shader != 0);
  return shader;
}

void Gl::deleteShader(GLuint shader)
{
  if (!_ok) return;
  _glDeleteShader(shader);
  check(__FUNCTION__);
}

void Gl::shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
  if (!_ok) return;
  _glShaderSource(shader, count, string, length);
  check(__FUNCTION__);
}

void Gl::compileShader(GLuint shader)
{
  if (!_ok) return;
  _glCompileShader(shader);
  check(__FUNCTION__);
}

void Gl::getShaderiv(GLuint shader, GLenum pname, GLint* params)
{
  if (!_ok) return;
  getShaderiv(shader, pname, params);
  check(__FUNCTION__);
}

void Gl::getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
  if (!_ok) return;
  _glGetShaderInfoLog(shader, maxLength, length, infoLog);
  check(__FUNCTION__);
}

GLuint Gl::createShader(GLenum shaderType, const char* source)
{
  if (!_ok) return 0;

	GLuint shader = createShader(shaderType);
	shaderSource(shader, 1, &source, NULL);
	compileShader(shader);

	GLint status;
	getShaderiv(shader, GL_COMPILE_STATUS, &status);

	if (status == GL_FALSE)
  {
		char buffer[4096];
		getShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
    _logger->printf(RETRO_LOG_ERROR, "Error in shader: %s", buffer);
    deleteShader(shader);
    return 0;
	}

  if (!_ok)
  {
    deleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint Gl::createProgram()
{
  if (!_ok) return 0;
  GLuint program = _glCreateProgram();
  check(__FUNCTION__, program != 0);
  return program;
}

void Gl::deleteProgram(GLuint program)
{
  if (!_ok) return;
  _glDeleteProgram(program);
  check(__FUNCTION__);
}

void Gl::attachShader(GLuint program, GLuint shader)
{
  if (!_ok) return;
  _glAttachShader(program, shader);
  check(__FUNCTION__);
}

void Gl::linkProgram(GLuint program)
{
  if (!_ok) return;
  _glLinkProgram(program);
  check(__FUNCTION__);
}

void Gl::validateProgram(GLuint program)
{
  if (!_ok) return;
  _glValidateProgram(program);
  check(__FUNCTION__);
}

void Gl::getProgramiv(GLuint program, GLenum pname, GLint* params)
{
  if (!_ok) return;
  getProgramiv(program, pname, params);
  check(__FUNCTION__);
}

void Gl::getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
  if (!_ok) return;
  _glGetProgramInfoLog(program, maxLength, length, infoLog);
  check(__FUNCTION__);
}

void Gl::useProgram(GLuint program)
{
  if (!_ok) return;
  _glUseProgram(program);
  check(__FUNCTION__);
}

GLint Gl::getAttribLocation(GLuint program, const GLchar* name)
{
  if (!_ok) return -1;
  GLint location = _glGetAttribLocation(program, name);
  check(__FUNCTION__, location != -1);
  return location;
}

GLint Gl::getUniformLocation(GLuint program, const GLchar* name)
{
  if (!_ok) return -1;
  GLint location = _glGetUniformLocation(program, name);
  check(__FUNCTION__, location != -1);
  return location;
}

void Gl::uniform2f(GLint location, GLfloat v0, GLfloat v1)
{
  if (!_ok) return;
  _glUniform2f(location, v0, v1);
  check(__FUNCTION__);
}

GLuint Gl::createProgram(const char* vertexShader, const char* fragmentShader)
{
  if (!_ok) return 0;

  GLuint vs = createShader(GL_VERTEX_SHADER, vertexShader);
  GLuint fs = createShader(GL_FRAGMENT_SHADER, fragmentShader);
  GLuint program = createProgram();

  attachShader(program, vs);
  attachShader(program, fs);
  linkProgram(program);

  deleteShader(vs);
  deleteShader(fs);

  validateProgram(program);

  GLint status;
	getProgramiv(program, GL_LINK_STATUS, &status);

	if (status == GL_FALSE)
  {
		char buffer[4096];
		getProgramInfoLog(program, sizeof(buffer), NULL, buffer);
    _logger->printf(RETRO_LOG_ERROR, "Error in shader program: %s", buffer);
    deleteProgram(program);
    return 0;
  }

  if (!_ok)
  {
    deleteProgram(program);
    return 0;
  }

  return program;
}

void Gl::genFramebuffers(GLsizei n, GLuint* ids)
{
  if (!_ok) return;
  _glGenFramebuffers(n, ids);
  check(__FUNCTION__);
}

void Gl::deleteFramebuffers(GLsizei n, GLuint* ids)
{
  if (!_ok) return;
  _glDeleteFramebuffers(n, ids);
  check(__FUNCTION__);
}

void Gl::bindFramebuffer(GLenum target, GLuint framebuffer)
{
  if (!_ok) return;
  _glBindFramebuffer(target, framebuffer);
  check(__FUNCTION__);
}

void Gl::framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  if (!_ok) return;
  _glFramebufferTexture2D(target, attachment, textarget, texture, level);
  check(__FUNCTION__);
}

void Gl::framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
  if (!_ok) return;
  _glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
  check(__FUNCTION__);
}

void Gl::drawBuffers(GLsizei n, const GLenum* bufs)
{
  if (!_ok) return;
  _glDrawBuffers(n, bufs);
  check(__FUNCTION__);
}

void Gl::genRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
  if (!_ok) return;
  _glGenRenderbuffers(n, renderbuffers);
  check(__FUNCTION__);
}

void Gl::deleteRenderbuffers(GLsizei n, const GLuint* renderbuffers)
{
  if (!_ok) return;
  _glDeleteRenderbuffers(n, renderbuffers);
  check(__FUNCTION__);
}

void Gl::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
  if (!_ok) return;
  _glBindRenderbuffer(target, renderbuffer);
  check(__FUNCTION__);
}

void Gl::renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
  if (!_ok) return;
  _glRenderbufferStorage(target, internalformat, width, height);
  check(__FUNCTION__);
}

void Gl::clearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  if (!_ok) return;
  glClearColor(red, green, blue, alpha);
  check(__FUNCTION__);
}

void Gl::clear(GLbitfield mask)
{
  if (!_ok) return;
  glClear(mask);
  check(__FUNCTION__);
}

void Gl::enable(GLenum cap)
{
  if (!_ok) return;
  glEnable(cap);
  check(__FUNCTION__);
}

void Gl::disable(GLenum cap)
{
  if (!_ok) return;
  glDisable(cap);
  check(__FUNCTION__);
}

void Gl::blendFunc(GLenum sfactor, GLenum dfactor)
{
  if (!_ok) return;
  glBlendFunc(sfactor, dfactor);
  check(__FUNCTION__);
}

void Gl::drawArrays(GLenum mode, GLint first, GLsizei count)
{
  if (!_ok) return;
  glDrawArrays(mode, first, count);
  check(__FUNCTION__);
}

void Gl::reset()
{
  check(__FUNCTION__);
  _ok = true;
}

void* Gl::getProcAddress(const char* symbol)
{
  if (!_ok) return NULL;

  void* address = SDL_GL_GetProcAddress(symbol);

  if (address == NULL)
  {
    _ok = false;
    _logger->printf(RETRO_LOG_ERROR, "Error in %s: symbol %s not found", __FUNCTION__, symbol);
  }

  return address;
}

void Gl::check(const char* function, bool ok)
{
  GLenum err = glGetError();

  if (err != GL_NO_ERROR || !ok)
  {
    _ok = false;
    _logger->printf(RETRO_LOG_ERROR, "Error in %s:", function);
    
    do
    {
      switch (err)
      {
      case GL_INVALID_OPERATION:
        _logger->printf(RETRO_LOG_ERROR, "  INVALID_OPERATION");
        break;

      case GL_INVALID_ENUM:
        _logger->printf(RETRO_LOG_ERROR, "  INVALID_ENUM");
        break;

      case GL_INVALID_VALUE:
        _logger->printf(RETRO_LOG_ERROR, "  INVALID_VALUE");
        break;

      case GL_OUT_OF_MEMORY:
        _logger->printf(RETRO_LOG_ERROR, "  OUT_OF_MEMORY");
        break;

      case GL_INVALID_FRAMEBUFFER_OPERATION:
        _logger->printf(RETRO_LOG_ERROR, "  INVALID_FRAMEBUFFER_OPERATION");
        break;
      }

      err = glGetError();
    }
    while (err != GL_NO_ERROR);
  }
}
