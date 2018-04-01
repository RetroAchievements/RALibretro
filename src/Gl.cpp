#include "Gl.h"

#include <SDL_video.h>
#include <SDL_opengl_glext.h>

static libretro::LoggerComponent* s_logger;
static bool s_ok;

static PFNGLACTIVETEXTUREPROC s_glActiveTexture;

static PFNGLGENBUFFERSPROC s_glGenBuffers;
static PFNGLDELETEBUFFERSPROC s_glDeleteBuffers;
static PFNGLBINDBUFFERPROC s_glBindBuffer;
static PFNGLBUFFERDATAPROC s_glBufferData;
static PFNGLVERTEXATTRIBPOINTERPROC s_glVertexAttribPointer;
static PFNGLENABLEVERTEXATTRIBARRAYPROC s_glEnableVertexAttribArray;

static PFNGLCREATESHADERPROC s_glCreateShader;
static PFNGLDELETESHADERPROC s_glDeleteShader;
static PFNGLSHADERSOURCEPROC s_glShaderSource;
static PFNGLCOMPILESHADERPROC s_glCompileShader;
static PFNGLGETSHADERIVPROC s_glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC s_glGetShaderInfoLog;

static PFNGLCREATEPROGRAMPROC s_glCreateProgram;
static PFNGLDELETEPROGRAMPROC s_glDeleteProgram;
static PFNGLATTACHSHADERPROC s_glAttachShader;
static PFNGLLINKPROGRAMPROC s_glLinkProgram;
static PFNGLVALIDATEPROGRAMPROC s_glValidateProgram;
static PFNGLGETPROGRAMIVPROC s_glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC s_glGetProgramInfoLog;
static PFNGLUSEPROGRAMPROC s_glUseProgram;
static PFNGLGETATTRIBLOCATIONPROC s_glGetAttribLocation;
static PFNGLGETUNIFORMLOCATIONPROC s_glGetUniformLocation;
static PFNGLUNIFORM1IPROC s_glUniform1i;
static PFNGLUNIFORM2FPROC s_glUniform2f;

static PFNGLGENFRAMEBUFFERSPROC s_glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC s_glDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC s_glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC s_glFramebufferTexture2D;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC s_glFramebufferRenderbuffer;
static PFNGLDRAWBUFFERSPROC s_glDrawBuffers;

static PFNGLGENRENDERBUFFERSPROC s_glGenRenderbuffers;
static PFNGLDELETERENDERBUFFERSPROC s_glDeleteRenderbuffers;
static PFNGLBINDRENDERBUFFERPROC s_glBindRenderbuffer;
static PFNGLRENDERBUFFERSTORAGEPROC s_glRenderbufferStorage;

static void* getProcAddress(const char* symbol)
{
  void* address = SDL_GL_GetProcAddress(symbol);

  if (address == NULL)
  {
    s_logger->printf(RETRO_LOG_ERROR, "Error in %s: symbol %s not found", __FUNCTION__, symbol);
  }

  return address;
}

static void check(const char* function, bool ok = true)
{
  GLenum err = glGetError();

  if (err != GL_NO_ERROR || !ok)
  {
    s_ok = false;
    s_logger->printf(RETRO_LOG_ERROR, "Error in %s:", function);

    do
    {
      switch (err)
      {
      case GL_INVALID_OPERATION:
        s_logger->printf(RETRO_LOG_ERROR, "  INVALID_OPERATION");
        break;

      case GL_INVALID_ENUM:
        s_logger->printf(RETRO_LOG_ERROR, "  INVALID_ENUM");
        break;

      case GL_INVALID_VALUE:
        s_logger->printf(RETRO_LOG_ERROR, "  INVALID_VALUE");
        break;

      case GL_OUT_OF_MEMORY:
        s_logger->printf(RETRO_LOG_ERROR, "  OUT_OF_MEMORY");
        break;

      case GL_INVALID_FRAMEBUFFER_OPERATION:
        s_logger->printf(RETRO_LOG_ERROR, "  INVALID_FRAMEBUFFER_OPERATION");
        break;
      }

      err = glGetError();
    } while (err != GL_NO_ERROR);
  }
}

void Gl::init(libretro::LoggerComponent* logger)
{
  s_logger = logger;
  s_ok = true;

  s_glActiveTexture = (PFNGLACTIVETEXTUREPROC)getProcAddress("glActiveTexture");
  
  s_glGenBuffers = (PFNGLGENBUFFERSPROC)getProcAddress("glGenBuffers");
  s_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)getProcAddress("glDeleteBuffers");
  s_glBindBuffer = (PFNGLBINDBUFFERPROC)getProcAddress("glBindBuffer");
  s_glBufferData = (PFNGLBUFFERDATAPROC)getProcAddress("glBufferData");
  s_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)getProcAddress("glVertexAttribPointer");
  s_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)getProcAddress("glEnableVertexAttribArray");

  s_glCreateShader = (PFNGLCREATESHADERPROC)getProcAddress("glCreateShader");
  s_glDeleteShader = (PFNGLDELETESHADERPROC)getProcAddress("glDeleteShader");
  s_glShaderSource = (PFNGLSHADERSOURCEPROC)getProcAddress("glShaderSource");
  s_glCompileShader = (PFNGLCOMPILESHADERPROC)getProcAddress("glCompileShader");
  s_glGetShaderiv = (PFNGLGETSHADERIVPROC)getProcAddress("glGetShaderiv");
  s_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)getProcAddress("glGetShaderInfoLog");

  s_glCreateProgram = (PFNGLCREATEPROGRAMPROC)getProcAddress("glCreateProgram");
  s_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)getProcAddress("glDeleteProgram");
  s_glAttachShader = (PFNGLATTACHSHADERPROC)getProcAddress("glAttachShader");
  s_glLinkProgram = (PFNGLLINKPROGRAMPROC)getProcAddress("glLinkProgram");
  s_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)getProcAddress("glValidateProgram");
  s_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)getProcAddress("glGetProgramiv");
  s_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)getProcAddress("glGetProgramInfoLog");
  s_glUseProgram = (PFNGLUSEPROGRAMPROC)getProcAddress("glUseProgram");
  s_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)getProcAddress("glGetAttribLocation");
  s_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)getProcAddress("glGetUniformLocation");
  s_glUniform1i = (PFNGLUNIFORM1IPROC)getProcAddress("glUniform1i");
  s_glUniform2f = (PFNGLUNIFORM2FPROC)getProcAddress("glUniform2f");

  s_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)getProcAddress("glGenFramebuffers");
  s_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)getProcAddress("glDeleteFramebuffers");
  s_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)getProcAddress("glBindRenderbuffer");
  s_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)getProcAddress("glBindFramebuffer");
  s_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)getProcAddress("glFramebufferTexture2D");
  s_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)getProcAddress("glRenderbufferStorage");
  s_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)getProcAddress("glFramebufferRenderbuffer");
  s_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)getProcAddress("glDrawBuffers");

  s_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)getProcAddress("glGenRenderbuffers");
  s_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)getProcAddress("glDeleteRenderbuffers");
}

bool Gl::ok()
{
  return s_ok;
}

void Gl::genTextures(GLsizei n, GLuint* textures)
{
  if (!s_ok) return;
  glGenTextures(n, textures);
  check(__FUNCTION__);
}

void Gl::deleteTextures(GLsizei n, const GLuint* textures)
{
  if (!s_ok) return;
  glDeleteTextures(n, textures);
  check(__FUNCTION__);
}

void Gl::bindTexture(GLenum target, GLuint texture)
{
  if (!s_ok) return;
  glBindTexture(target, texture);
  check(__FUNCTION__);
}

void Gl::activeTexture(GLenum texture)
{
  if (!s_ok) return;
  s_glActiveTexture(texture);
  check(__FUNCTION__);
}

void Gl::texParameteri(GLenum target, GLenum pname, GLint param)
{
  if (!s_ok) return;
  glTexParameteri(target, pname, param);
  check(__FUNCTION__);
}

void Gl::texImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data)
{
  if (!s_ok) return;
  glTexImage2D(target, level, internalFormat, width, height, border, format, type, data);
  check(__FUNCTION__);
}

void Gl::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels)
{
  if (!s_ok) return;
  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
  check(__FUNCTION__);
}

void Gl::getTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid* pixels)
{
  if (!s_ok) return;
  glGetTexImage(target, level, format, type, pixels);
  check(__FUNCTION__);
}

void Gl::genBuffers(GLsizei n, GLuint* buffers)
{
  if (!s_ok || s_glGenBuffers == NULL) return;
  s_glGenBuffers(n, buffers);
  check(__FUNCTION__);
}

void Gl::deleteBuffers(GLsizei n, const GLuint* buffers)
{
  if (!s_ok || s_glDeleteBuffers == NULL) return;
  s_glDeleteBuffers(n, buffers);
  check(__FUNCTION__);
}

void Gl::bindBuffer(GLenum target, GLuint buffer)
{
  if (!s_ok || s_glBindBuffer == NULL) return;
  s_glBindBuffer(target, buffer);
  check(__FUNCTION__);
}

void Gl::bufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
  if (!s_ok || s_glBufferData == NULL) return;
  s_glBufferData(target, size, data, usage);
  check(__FUNCTION__);
}

void Gl::vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer)
{
  if (!s_ok || s_glVertexAttribPointer == NULL) return;
  s_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
  check(__FUNCTION__);
}

void Gl::enableVertexAttribArray(GLuint index)
{
  if (!s_ok || s_glEnableVertexAttribArray == NULL) return;
  s_glEnableVertexAttribArray(index);
  check(__FUNCTION__);
}

GLuint Gl::createShader(GLenum shaderType)
{
  if (!s_ok || s_glCreateShader == NULL) return 0;
  GLuint shader = s_glCreateShader(shaderType);
  check(__FUNCTION__, shader != 0);
  return shader;
}

void Gl::deleteShader(GLuint shader)
{
  if (!s_ok || s_glDeleteShader == NULL) return;
  s_glDeleteShader(shader);
  check(__FUNCTION__);
}

void Gl::shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
  if (!s_ok || s_glShaderSource == NULL) return;
  s_glShaderSource(shader, count, string, length);
  check(__FUNCTION__);
}

void Gl::compileShader(GLuint shader)
{
  if (!s_ok || s_glCompileShader == NULL) return;
  s_glCompileShader(shader);
  check(__FUNCTION__);
}

void Gl::getShaderiv(GLuint shader, GLenum pname, GLint* params)
{
  if (!s_ok || s_glGetShaderiv == NULL) return;
  s_glGetShaderiv(shader, pname, params);
  check(__FUNCTION__);
}

void Gl::getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
  if (!s_ok || s_glGetShaderInfoLog == NULL) return;
  s_glGetShaderInfoLog(shader, maxLength, length, infoLog);
  check(__FUNCTION__);
}

GLuint Gl::createShader(GLenum shaderType, const char* source)
{
  if (!s_ok) return 0;

	GLuint shader = createShader(shaderType);
	shaderSource(shader, 1, &source, NULL);
	compileShader(shader);

	GLint status;
	getShaderiv(shader, GL_COMPILE_STATUS, &status);

	if (status == GL_FALSE)
  {
		char buffer[4096];
		getShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
    s_logger->printf(RETRO_LOG_ERROR, "Error in shader: %s", buffer);
    deleteShader(shader);
    return 0;
	}

  if (!s_ok)
  {
    deleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint Gl::createProgram()
{
  if (!s_ok || s_glCreateProgram == NULL) return 0;
  GLuint program = s_glCreateProgram();
  check(__FUNCTION__, program != 0);
  return program;
}

void Gl::deleteProgram(GLuint program)
{
  if (!s_ok || s_glDeleteProgram == NULL) return;
  s_glDeleteProgram(program);
  check(__FUNCTION__);
}

void Gl::attachShader(GLuint program, GLuint shader)
{
  if (!s_ok || s_glAttachShader == NULL) return;
  s_glAttachShader(program, shader);
  check(__FUNCTION__);
}

void Gl::linkProgram(GLuint program)
{
  if (!s_ok || s_glLinkProgram == NULL) return;
  s_glLinkProgram(program);
  check(__FUNCTION__);
}

void Gl::validateProgram(GLuint program)
{
  if (!s_ok || s_glValidateProgram == NULL) return;
  s_glValidateProgram(program);
  check(__FUNCTION__);
}

void Gl::getProgramiv(GLuint program, GLenum pname, GLint* params)
{
  if (!s_ok || s_glGetProgramiv == NULL) return;
  s_glGetProgramiv(program, pname, params);
  check(__FUNCTION__);
}

void Gl::getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
  if (!s_ok || s_glGetProgramInfoLog == NULL) return;
  s_glGetProgramInfoLog(program, maxLength, length, infoLog);
  check(__FUNCTION__);
}

void Gl::useProgram(GLuint program)
{
  if (!s_ok || s_glUseProgram == NULL) return;
  s_glUseProgram(program);
  check(__FUNCTION__);
}

GLint Gl::getAttribLocation(GLuint program, const GLchar* name)
{
  if (!s_ok || s_glGetAttribLocation == NULL) return -1;
  GLint location = s_glGetAttribLocation(program, name);
  check(__FUNCTION__, location != -1);
  return location;
}

GLint Gl::getUniformLocation(GLuint program, const GLchar* name)
{
  if (!s_ok || s_glGetUniformLocation == NULL) return -1;
  GLint location = s_glGetUniformLocation(program, name);
  check(__FUNCTION__, location != -1);
  return location;
}

void Gl::uniform1i(GLint location, GLint v0)
{
  if (!s_ok || s_glUniform1i == NULL) return;
  s_glUniform1i(location, v0);
  check(__FUNCTION__);
}

void Gl::uniform2f(GLint location, GLfloat v0, GLfloat v1)
{
  if (!s_ok || s_glUniform2f == NULL) return;
  s_glUniform2f(location, v0, v1);
  check(__FUNCTION__);
}

GLuint Gl::createProgram(const char* vertexShader, const char* fragmentShader)
{
  if (!s_ok) return 0;

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
    s_logger->printf(RETRO_LOG_ERROR, "Error in shader program: %s", buffer);
    deleteProgram(program);
    return 0;
  }

  if (!s_ok)
  {
    deleteProgram(program);
    return 0;
  }

  return program;
}

void Gl::genFramebuffers(GLsizei n, GLuint* ids)
{
  if (!s_ok || s_glGenFramebuffers == NULL) return;
  s_glGenFramebuffers(n, ids);
  check(__FUNCTION__);
}

void Gl::deleteFramebuffers(GLsizei n, GLuint* ids)
{
  if (!s_ok || s_glDeleteFramebuffers == NULL) return;
  s_glDeleteFramebuffers(n, ids);
  check(__FUNCTION__);
}

void Gl::bindFramebuffer(GLenum target, GLuint framebuffer)
{
  if (!s_ok || s_glBindFramebuffer == NULL) return;
  s_glBindFramebuffer(target, framebuffer);
  check(__FUNCTION__);
}

void Gl::framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  if (!s_ok || s_glFramebufferTexture2D == NULL) return;
  s_glFramebufferTexture2D(target, attachment, textarget, texture, level);
  check(__FUNCTION__);
}

void Gl::framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
  if (!s_ok || s_glFramebufferRenderbuffer == NULL) return;
  s_glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
  check(__FUNCTION__);
}

void Gl::drawBuffers(GLsizei n, const GLenum* bufs)
{
  if (!s_ok || s_glDrawBuffers == NULL) return;
  s_glDrawBuffers(n, bufs);
  check(__FUNCTION__);
}

void Gl::genRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
  if (!s_ok || s_glGenRenderbuffers == NULL) return;
  s_glGenRenderbuffers(n, renderbuffers);
  check(__FUNCTION__);
}

void Gl::deleteRenderbuffers(GLsizei n, const GLuint* renderbuffers)
{
  if (!s_ok || s_glDeleteRenderbuffers == NULL) return;
  s_glDeleteRenderbuffers(n, renderbuffers);
  check(__FUNCTION__);
}

void Gl::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
  if (!s_ok || s_glBindRenderbuffer == NULL) return;
  s_glBindRenderbuffer(target, renderbuffer);
  check(__FUNCTION__);
}

void Gl::renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
  if (!s_ok || s_glRenderbufferStorage == NULL) return;
  s_glRenderbufferStorage(target, internalformat, width, height);
  check(__FUNCTION__);
}

void Gl::clearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  if (!s_ok) return;
  glClearColor(red, green, blue, alpha);
  check(__FUNCTION__);
}

void Gl::clear(GLbitfield mask)
{
  if (!s_ok) return;
  glClear(mask);
  check(__FUNCTION__);
}

void Gl::enable(GLenum cap)
{
  if (!s_ok) return;
  glEnable(cap);
  check(__FUNCTION__);
}

void Gl::disable(GLenum cap)
{
  if (!s_ok) return;
  glDisable(cap);
  check(__FUNCTION__);
}

void Gl::blendFunc(GLenum sfactor, GLenum dfactor)
{
  if (!s_ok) return;
  glBlendFunc(sfactor, dfactor);
  check(__FUNCTION__);
}

void Gl::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
  if (!s_ok) return;
  glViewport(x, y, width, height);
  check(__FUNCTION__);
}

void Gl::drawArrays(GLenum mode, GLint first, GLsizei count)
{
  if (!s_ok) return;
  glDrawArrays(mode, first, count);
  check(__FUNCTION__);
}
