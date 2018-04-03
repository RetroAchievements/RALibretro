/*
Copyright (C) 2018 Andre Leiradella

This file is part of RALibretro.

RALibretro is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RALibretro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Video.h"
#include "Gl.h"

#include "Dialog.h"
#include "jsonsax/jsonsax.h"

#include <SDL_render.h>

#include <math.h>

bool Video::init(libretro::LoggerComponent* logger, Config* config)
{
  _logger = logger;
  _config = config;

  _preserveAspect = false;
  _linearFilter = false;
  _pixelFormat = RETRO_PIXEL_FORMAT_UNKNOWN;
  _windowWidth = _windowHeight = 0;
  _textureWidth = _textureHeight = 0;
  _viewWidth = _viewHeight = 0;

  _vertexBuffer = 0;
  _texture = 0;
  _framebuffer = 0;
  _renderbuffer = 0;

  _program = createProgram(&_posAttribute, &_uvAttribute, &_texUniform);

  if (!Gl::ok())
  {
    destroy();
    return false;
  }

  return true;
}

void Video::destroy()
{
  if (_renderbuffer != 0)
  {
    Gl::deleteRenderbuffers(1, &_renderbuffer);
    _renderbuffer = 0;
  }

  if (_framebuffer != 0)
  {
    Gl::deleteFramebuffers(1, &_framebuffer);
    _framebuffer = 0;
  }

  if (_texture != 0)
  {
    Gl::deleteTextures(1, &_texture);
    _texture = 0;
  }

  if (_vertexBuffer != 0)
  {
    Gl::deleteBuffers(1, &_vertexBuffer);
    _vertexBuffer = 0;
  }

  if (_program != 0)
  {
    Gl::deleteProgram(_program);
    _program = 0;
  }
}

void Video::draw()
{
  if (_texture != 0)
  {
    float height = (float)_windowHeight;
    float width = height * _aspect;

    if (width > _windowWidth)
    {
      width = (float)_windowWidth;
      height = width / _aspect;
    }

    Gl::useProgram(_program);

    Gl::enableVertexAttribArray(_posAttribute);
    Gl::enableVertexAttribArray(_uvAttribute);

    Gl::activeTexture(GL_TEXTURE0);
    Gl::bindTexture(GL_TEXTURE_2D, _texture);
    Gl::uniform1i(_texUniform, 0);

    Gl::drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
}

bool Video::setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback)
{
  (void)width;
  (void)height;
  (void)hwRenderCallback;

  if (pixelFormat != _pixelFormat && _texture != 0)
  {
    Gl::deleteTextures(1, &_texture);
    _textureWidth = _textureHeight = 0;
  }

  _aspect = aspect;
  _pixelFormat = pixelFormat;

  _logger->printf(RETRO_LOG_DEBUG, "Geometry set to %u x %u (1:%f)", width, height, aspect);

#if 0
  if (hwRenderCallback != NULL)
  {
    // Hardware framebuffer
    Gl::genFramebuffers(1, &_framebuffer);
    Gl::bindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

    Gl::framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);

    Gl::genRenderbuffers(1, &_renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);
    GLenum internalFormat = hwRenderCallback->stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
    Gl::renderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
    Gl::framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _renderbuffer);

    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    Gl::drawBuffers(sizeof(drawBuffers) / sizeof(drawBuffers[0]), drawBuffers);

    if (!Gl::ok())
    {
      destroy();
      return false;
    }
  }
#endif

  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
  if (data != NULL && data != RETRO_HW_FRAME_BUFFER_VALID)
  {
    bool updateVertexBuffer = false;

    unsigned textureWidth = pitch;

    switch (_pixelFormat)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888: textureWidth /= 4; break;
    case RETRO_PIXEL_FORMAT_RGB565:   // fallthrough
    case RETRO_PIXEL_FORMAT_0RGB1555: // fallthrough
    default:                          textureWidth /= 2; break;
    }

    if (textureWidth > _textureWidth || height > _textureHeight)
    {
      if (_texture != 0)
      {
        Gl::deleteTextures(1, &_texture);
      }

      _texture = createTexture(textureWidth, height, _pixelFormat, _linearFilter);

      _textureWidth = textureWidth;
      _textureHeight = height;

      updateVertexBuffer = true;
    }

    Gl::bindTexture(GL_TEXTURE_2D, _texture);

    switch (_pixelFormat)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
      break;
      
    case RETRO_PIXEL_FORMAT_RGB565:
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
      break;
      
    case RETRO_PIXEL_FORMAT_0RGB1555:
    default:
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, height, GL_RGB, GL_UNSIGNED_SHORT_1_5_5_5_REV, data);
      break;
    }

    if (width != _viewWidth || height != _viewHeight)
    {
      _viewWidth = width;
      _viewHeight = height;

      updateVertexBuffer = true;
    }

    if (updateVertexBuffer)
    {
      float texScaleX = (float)_viewWidth / (float)_textureWidth;
      float texScaleY = (float)_viewHeight / (float)_textureHeight;

      Gl::deleteBuffers(1, &_vertexBuffer);
      _vertexBuffer = createVertexBuffer(_windowWidth, _windowHeight, texScaleX, texScaleY, _posAttribute, _uvAttribute);
    }
  }
}

bool Video::supportsContext(enum retro_hw_context_type type)
{
  // Do we really support those two?

  switch (type)
  {
  case RETRO_HW_CONTEXT_OPENGL:
  //case RETRO_HW_CONTEXT_OPENGLES2:
  case RETRO_HW_CONTEXT_OPENGL_CORE:
  //case RETRO_HW_CONTEXT_OPENGLES3:
  //case RETRO_HW_CONTEXT_OPENGLES_VERSION:
    return true;
  
  default:
    return false;
  }
}

uintptr_t Video::getCurrentFramebuffer()
{
  return 0;
}

retro_proc_address_t Video::getProcAddress(const char* symbol)
{
  void* address = SDL_GL_GetProcAddress(symbol);
  _logger->printf(RETRO_LOG_DEBUG, "OpenGL symbol: %p resolved for %s", address, symbol);
  return (retro_proc_address_t)address;
}

void Video::showMessage(const char* msg, unsigned frames)
{
  _logger->printf(RETRO_LOG_INFO, "OSD message (%u): %s", frames, msg);
}

void Video::windowResized(unsigned width, unsigned height)
{
  _windowWidth = width;
  _windowHeight = height;
  Gl::viewport(0, 0, width, height);

  _logger->printf(RETRO_LOG_INFO, "Window resized to %u x %u", width, height);

  float texScaleX = (float)_viewWidth / (float)_textureWidth;
  float texScaleY = (float)_viewHeight / (float)_textureHeight;

  createVertexBuffer(width, height, texScaleX, texScaleY, _posAttribute, _uvAttribute);
}

void Video::getFramebufferSize(unsigned* width, unsigned* height)
{
  unsigned w, h;

  if (_preserveAspect)
  {
    h = _viewHeight;
    w = (unsigned)(_viewHeight * _aspect);

    if (w < _viewWidth)
    {
      w = _viewWidth;
      h = (unsigned)(_viewWidth / _aspect);
    }
  }
  else
  {
    w = _viewWidth;
    h = _viewHeight;
  }

  *width = w;
  *height = h;
}

const void* Video::getFramebuffer(unsigned* width, unsigned* height, unsigned* pitch, enum retro_pixel_format* format)
{
  unsigned bpp = _pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
  void* pixels = malloc(_textureWidth * _textureHeight * bpp);

  if (pixels == NULL)
  {
    return NULL;
  }

  Gl::bindTexture(GL_TEXTURE_2D, _texture);

  switch (_pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    Gl::getTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    break;
    
  case RETRO_PIXEL_FORMAT_RGB565:
    Gl::getTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
    break;
    
  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    Gl::getTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_SHORT_1_5_5_5_REV, pixels);
    break;
  }

  *width = _viewWidth;
  *height = _viewHeight;
  *pitch = _textureWidth * bpp;
  *format = _pixelFormat;

  return pixels;
}

std::string Video::serialize()
{
  std::string json("{");

  json.append("\"_preserveAspect\":");
  json.append(_preserveAspect ? "true" : "false");
  json.append(",");

  json.append("\"_linearFilter\":");
  json.append(_linearFilter ? "true" : "false");

  json.append("}");
  return json;
}

void Video::deserialize(const char* json)
{
  struct Deserialize
  {
    Video* self;
    std::string key;
  };

  Deserialize ud;
  ud.self = this;

  jsonsax_parse(json, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num) {
    auto ud = (Deserialize*)udata;

    if (event == JSONSAX_KEY)
    {
      ud->key = std::string(str, num);
    }
    else if (event == JSONSAX_BOOLEAN)
    {
      if (ud->key == "_preserveAspect")
      {
        ud->self->_preserveAspect = num != 0;
      }
      if (ud->key == "_linearFilter")
      {
        ud->self->_linearFilter = num != 0;
      }
    }

    return 0;
  });
}

void Video::showDialog()
{
  const WORD WIDTH = 280;
  const WORD LINE = 15;

  Dialog db;
  db.init("Video Settings");

  WORD y = 0;

  db.addCheckbox("Preserve aspect ratio", 51001, 0, y, WIDTH / 2 - 5, 8, &_preserveAspect);
  db.addCheckbox("Linear filtering", 51002, WIDTH / 2 + 5, y, WIDTH / 2 - 5, 8, &_linearFilter);

  y += LINE;

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  bool preserveAspect = _preserveAspect;
  bool linearFilter = _linearFilter;

  if (db.show())
  {
    if (linearFilter != _linearFilter)
    {
      if (_texture != 0)
      {
        Gl::deleteTextures(1, &_texture);
      }

      _texture = createTexture(_textureWidth, _textureHeight, _pixelFormat, _linearFilter);
    }

    if (preserveAspect != _preserveAspect)
    {
      if (_vertexBuffer != 0)
      {
        Gl::deleteBuffers(1, &_vertexBuffer);
      }

      float texScaleX = (float)_viewWidth / (float)_textureWidth;
      float texScaleY = (float)_viewHeight / (float)_textureHeight;

      Gl::deleteBuffers(1, &_vertexBuffer);
      _vertexBuffer = createVertexBuffer(_windowWidth, _windowHeight, texScaleX, texScaleY, _posAttribute, _uvAttribute);
    }
  }
}

GLuint Video::createProgram(GLint* pos, GLint* uv, GLint* tex)
{
  static const char* vertexShader =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "  v_uv = a_uv;\n"
    "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}";
  
  static const char* fragmentShader =
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_tex, v_uv);\n"
    "}";
  
  GLuint program = Gl::createProgram(vertexShader, fragmentShader);

  *pos = Gl::getAttribLocation(program, "a_pos");
  *uv = Gl::getAttribLocation(program, "a_uv");
  *tex = Gl::getUniformLocation(program, "u_tex");

  return program;
}

GLuint Video::createVertexBuffer(unsigned windowWidth, unsigned windowHeight, float texScaleX, float texScaleY, GLint pos, GLint uv)
{
  struct VertexData
  {
    float x, y, u, v;
  };

  float winScaleX, winScaleY;

  if (_preserveAspect)
  {
    unsigned h = windowHeight;
    unsigned w = (unsigned)(windowHeight * _aspect);

    if (w > windowWidth)
    {
      w = windowWidth;
      h = (unsigned)(windowWidth / _aspect);
    }

    winScaleX = (float)w / (float)windowWidth;
    winScaleY = (float)h / (float)windowHeight;
  }
  else
  {
    winScaleX = winScaleY = 1.0f;
  }

  const VertexData vertexData[] = {
    {-winScaleX, -winScaleY,      0.0f, texScaleY},
    {-winScaleX,  winScaleY,      0.0f,      0.0f},
    { winScaleX, -winScaleY, texScaleX, texScaleY},
    { winScaleX,  winScaleY, texScaleX,      0.0f}
  };
  
  GLuint vertexBuffer;
  Gl::genBuffers(1, &vertexBuffer);

  Gl::bindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  Gl::bufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
  
  Gl::vertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, x));
  Gl::vertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, u));

  _logger->printf(RETRO_LOG_DEBUG, "Vertices updated with window scale %f x %f and texture scale %f x %f", winScaleX, winScaleY, texScaleX, texScaleY);
  return vertexBuffer;
}

GLuint Video::createTexture(unsigned width, unsigned height, retro_pixel_format pixelFormat, bool linear)
{
  GLuint texture;
  Gl::genTextures(1, &texture);

  Gl::bindTexture(GL_TEXTURE_2D, texture);

  GLint filter = linear ? GL_LINEAR : GL_NEAREST;
  Gl::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  Gl::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

  const char* format;

  switch (pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    Gl::texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    format = "RGBA8888";
    break;

  case RETRO_PIXEL_FORMAT_RGB565:
    Gl::texImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    format = "RGB565";
    break;

  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    Gl::texImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
    format = "RGBA5551";
    break;
  }

  _logger->printf(RETRO_LOG_DEBUG, "Texture created with dimensions %u x %u (%s, %s)", width, height, format, linear ? "linear" : "nearest");
  return texture;
}
