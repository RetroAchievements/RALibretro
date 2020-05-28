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
#include "GlUtil.h"

#include "Dialog.h"
#include "jsonsax/jsonsax.h"

#include <SDL_render.h>

#include <math.h>

#define TAG "[VID] "

bool Video::init(libretro::LoggerComponent* logger, Config* config)
{
  _logger = logger;
  _config = config;

  _pixelFormat = RETRO_PIXEL_FORMAT_UNKNOWN;
  _windowWidth = _windowHeight = 0;
  _textureWidth = _textureHeight = 0;
  _viewWidth = _viewHeight = 0;

  _vertexArray = _vertexBuffer = 0;
  _texture = 0;
  _rotation = Rotation::None;
  _rotationHandler = NULL;

  _preserveAspect = false;
  _linearFilter = false;

  _program = createProgram(&_posAttribute, &_uvAttribute, &_texUniform);

  _hw.enabled = false;
  _hw.frameBuffer = _hw.renderBuffer = 0;
  _hw.callback = nullptr;

  if (!Gl::ok())
  {
    destroy();
    return false;
  }

  return true;
}

void Video::destroy()
{
  if (_texture != 0)
  {
    Gl::deleteTextures(1, &_texture);
    _texture = 0;
  }

  if (_vertexArray != 0)
  {
    Gl::deleteVertexArrays(1, &_vertexArray);
    _vertexArray = 0;
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

  if (_hw.frameBuffer != 0)
  {
    Gl::deleteFramebuffers(1, &_hw.frameBuffer);
    _hw.frameBuffer = 0;
  }

  if (_hw.renderBuffer != 0)
  {
    Gl::deleteRenderbuffers(1, &_hw.renderBuffer);
    _hw.renderBuffer = 0;
  }
}

void Video::draw()
{
  if (_texture != 0)
  {
    Gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Gl::useProgram(_program);

    Gl::bindVertexArray(_vertexArray);

    Gl::activeTexture(GL_TEXTURE0);
    Gl::bindTexture(GL_TEXTURE_2D, _texture);
    Gl::uniform1i(_texUniform, 0);

    Gl::drawArrays(GL_TRIANGLE_STRIP, 0, 4);

    Gl::bindTexture(GL_TEXTURE_2D, 0);
    Gl::bindVertexArray(0);
    Gl::useProgram(0);
  }
}

bool Video::setGeometry(unsigned width, unsigned height, unsigned maxWidth, unsigned maxHeight, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback)
{
  bool hardwareRender = hwRenderCallback != nullptr;
  if (!hardwareRender && _hw.enabled)
    postHwRenderReset();

  _hw.enabled = hardwareRender;
  _hw.callback = hwRenderCallback;

  if (!ensureFramebuffer(maxWidth, maxHeight, pixelFormat, _linearFilter))
    return false;

  _aspect = aspect;

  _logger->debug(TAG "Geometry set to %u x %u (max %u x %u) (1:%f)", width, height, maxWidth, maxHeight, aspect);
  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
  if (data == NULL)
  {
    _logger->debug(TAG "Refresh not performed, data is NULL");
  }
  else if (data != RETRO_HW_FRAME_BUFFER_VALID)
  {
    Gl::bindTexture(GL_TEXTURE_2D, _texture);

    unsigned rowLength = pitch;
    switch (_pixelFormat)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888: rowLength /= 4; break;
    case RETRO_PIXEL_FORMAT_RGB565:   // fallthrough
    case RETRO_PIXEL_FORMAT_0RGB1555: // fallthrough
    default:                          rowLength /= 2; break;
    }

    Gl::pixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    switch (_pixelFormat)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
      break;
      
    case RETRO_PIXEL_FORMAT_RGB565:
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
      break;
      
    case RETRO_PIXEL_FORMAT_0RGB1555:
    default:
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, data);
      break;
    }

    Gl::pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    Gl::bindTexture(GL_TEXTURE_2D, 0);

    _logger->debug(TAG "Texture refreshed with %u x %u pixels", width, height);

    ensureView(width, height, _windowWidth, _windowHeight, _preserveAspect, _rotation);
  }
  else if (_hw.enabled && data == RETRO_HW_FRAME_BUFFER_VALID)
  {
    postHwRenderReset();
    ensureView(width, height, _windowWidth, _windowHeight, _preserveAspect, _rotation);
  }
}

bool Video::supportsContext(enum retro_hw_context_type type)
{
  switch (type)
  {
    case RETRO_HW_CONTEXT_OPENGL:
    case RETRO_HW_CONTEXT_OPENGL_CORE:
      return true;
  }

  return false;
}

uintptr_t Video::getCurrentFramebuffer()
{
  _logger->debug(TAG "getCurrentFramebuffer() => %u", _hw.frameBuffer);
  return _hw.frameBuffer;
}

retro_proc_address_t Video::getProcAddress(const char* symbol)
{
  void* address = SDL_GL_GetProcAddress(symbol);
  _logger->debug(TAG "OpenGL symbol: %p resolved for %s", address, symbol);
  return (retro_proc_address_t)address;
}

void Video::showMessage(const char* msg, unsigned frames)
{
  _logger->info(TAG "OSD message (%u): %s", frames, msg);
}

void Video::windowResized(unsigned width, unsigned height)
{
  Gl::viewport(0, 0, width, height);
  ensureView(_viewWidth, _viewHeight, width, height, _preserveAspect, _rotation);
  _logger->debug(TAG "Window resized to %u x %u", width, height);
}

void Video::getFramebufferSize(unsigned* width, unsigned* height, enum retro_pixel_format* format)
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
  *format = _pixelFormat;
}

static void verticalFlipRawTexture(uint8_t *data, unsigned height, unsigned pitch)
{
  uint8_t *swapSpace = (uint8_t*)malloc(pitch);
  for (uint8_t *top = data, *bottom = (data + ((height - 1) * pitch));
    top < bottom;
    top += pitch, bottom -= pitch)
  {
    memcpy(swapSpace, top, pitch);
    memcpy(top, bottom, pitch);
    memcpy(bottom, swapSpace, pitch);
  }
  free(swapSpace);
}

const void* Video::getFramebuffer(unsigned* width, unsigned* height, unsigned* pitch, enum retro_pixel_format* format)
{
  unsigned bpp = _pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
  uint8_t* pixels = (uint8_t*)malloc(_textureWidth * _textureHeight * bpp);

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
    Gl::getTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, pixels);
    break;
  }

  if (_hw.enabled && _hw.callback->bottom_left_origin)
    verticalFlipRawTexture(pixels, _textureHeight, _textureWidth * bpp);

  *width = _viewWidth;
  *height = _viewHeight;
  *pitch = _textureWidth * bpp;
  *format = _pixelFormat;

  return pixels;
}

void Video::setFramebuffer(void* pixels, unsigned width, unsigned height, unsigned pitch)
{
  auto p = (uint8_t*)pixels;

  if (_hw.enabled && _hw.callback->bottom_left_origin)
    verticalFlipRawTexture(p, height, pitch);

  Gl::bindTexture(GL_TEXTURE_2D, _texture);

  switch (_pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    for (unsigned y = 0; y < height; y++, p += pitch)
    {
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, p);
    }

    break;
    
  case RETRO_PIXEL_FORMAT_RGB565:
    for (unsigned y = 0; y < height; y++, p += pitch)
    {
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, p);
    }

    break;
    
  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    for (unsigned y = 0; y < height; y++, p += pitch)
    {
      Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, p);
    }

    break;
  }
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

static const char* s_getRotateOptions(int index, void* udata)
{
  switch (index)
  {
    case 0: return "None";
    case 1: return "90";
    case 2: return "180";
    case 3: return "270";
    default: return NULL;
  }
}

void Video::showDialog()
{
  const WORD WIDTH = 140;
  const WORD LINE = 15;

  Dialog db;
  db.init("Video Settings");

  WORD y = 0;

  bool preserveAspect = _preserveAspect;
  db.addCheckbox("Preserve aspect ratio", 51001, 0, y, WIDTH - 10, 8, &preserveAspect);
  y += LINE;

  bool linearFilter = _linearFilter;
  db.addCheckbox("Linear filtering", 51002, 0, y, WIDTH - 10, 8, &linearFilter);
  y += LINE;

  int rotation = (int)_rotation;
  db.addLabel("Screen Rotation", 51003, 0, y, 50, 8);
  db.addCombobox(51004, 55, y - 2, WIDTH - 55, 12, 100, s_getRotateOptions, NULL, &rotation);
  y += LINE;

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  if (db.show())
  {
    ensureFramebuffer(_textureWidth, _textureHeight, _pixelFormat, linearFilter);
    ensureView(_viewWidth, _viewHeight, _windowWidth, _windowHeight, preserveAspect, static_cast<Rotation>(rotation));
  }
}

void Video::setRotation(Rotation rotation)
{
  ensureView(_viewWidth, _viewHeight, _windowWidth, _windowHeight, _preserveAspect, rotation);
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
  
  GLuint program = GlUtil::createProgram(vertexShader, fragmentShader);

  *pos = Gl::getAttribLocation(program, "a_pos");
  *uv = Gl::getAttribLocation(program, "a_uv");
  *tex = Gl::getUniformLocation(program, "u_tex");

  return program;
}

bool Video::ensureVertexArray(unsigned windowWidth, unsigned windowHeight, float texScaleX, float texScaleY, GLint pos, GLint uv)
{
  struct VertexData
  {
    float x, y, u, v;
  };

  float winScaleX, winScaleY;

  if (_preserveAspect)
  {
    unsigned w, h;
    switch (_rotation)
    {
      default:
      case Rotation::None:
      case Rotation::OneEighty:
        h = windowHeight;
        w = (unsigned)(windowHeight * _aspect);
        if (w > windowWidth)
        {
          w = windowWidth;
          h = (unsigned)(windowWidth / _aspect);
        }
        break;

      case Rotation::Ninety:
      case Rotation::TwoSeventy:
        w = windowWidth;
        h = (unsigned)(windowWidth * _aspect);
        if (h > windowHeight)
        {
          h = windowHeight;
          w = (unsigned)(windowHeight / _aspect);
        }
        break;
    }

    winScaleX = (float)w / (float)windowWidth;
    winScaleY = (float)h / (float)windowHeight;
  }
  else
  {
    winScaleX = winScaleY = 1.0f;
  }

  if (_hw.enabled && _hw.callback->bottom_left_origin)
  {
    if (_rotation == Rotation::Ninety || _rotation == Rotation::TwoSeventy)
      winScaleX = -winScaleX;
    else
      winScaleY = -winScaleY;
  }

  const VertexData vertexData0[] = {
    {-winScaleX, -winScaleY,      0.0f, texScaleY},
    {-winScaleX,  winScaleY,      0.0f,      0.0f},
    { winScaleX, -winScaleY, texScaleX, texScaleY},
    { winScaleX,  winScaleY, texScaleX,      0.0f}
  };
  const VertexData vertexData90[] = {
    {-winScaleX, -winScaleY,      0.0f,      0.0f},
    {-winScaleX,  winScaleY, texScaleX,      0.0f},
    { winScaleX, -winScaleY,      0.0f, texScaleY},
    { winScaleX,  winScaleY, texScaleX, texScaleY}
  };
  const VertexData vertexData180[] = {
    {-winScaleX, -winScaleY, texScaleX,      0.0f},
    {-winScaleX,  winScaleY, texScaleX, texScaleY},
    { winScaleX, -winScaleY,      0.0f,      0.0f},
    { winScaleX,  winScaleY,      0.0f, texScaleY}
  };
  const VertexData vertexData270[] = {
    {-winScaleX, -winScaleY, texScaleX, texScaleY},
    {-winScaleX,  winScaleY,      0.0f, texScaleY},
    { winScaleX, -winScaleY, texScaleX,      0.0f},
    { winScaleX,  winScaleY,      0.0f,      0.0f}
  };

  if (_vertexArray != 0)
    Gl::deleteVertexArrays(1, &_vertexArray);
  if (_vertexBuffer != 0)
    Gl::deleteBuffers(1, &_vertexBuffer);

  Gl::genVertexArray(1, &_vertexArray);
  if (_vertexArray == 0)
    return false;
  Gl::bindVertexArray(_vertexArray);

  Gl::genBuffers(1, &_vertexBuffer);
  if (_vertexBuffer == 0)
    return false;
  Gl::bindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);

  switch (_rotation)
  {
    default:
      Gl::bufferData(GL_ARRAY_BUFFER, sizeof(vertexData0), vertexData0, GL_STATIC_DRAW);
      break;
    case Rotation::Ninety:
      Gl::bufferData(GL_ARRAY_BUFFER, sizeof(vertexData90), vertexData90, GL_STATIC_DRAW);
      break;
    case Rotation::OneEighty:
      Gl::bufferData(GL_ARRAY_BUFFER, sizeof(vertexData180), vertexData180, GL_STATIC_DRAW);
      break;
    case Rotation::TwoSeventy:
      Gl::bufferData(GL_ARRAY_BUFFER, sizeof(vertexData270), vertexData270, GL_STATIC_DRAW);
      break;
  }

  Gl::enableVertexAttribArray(pos);
  Gl::vertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, x));
  Gl::enableVertexAttribArray(uv);
  Gl::vertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, u));

  Gl::bindVertexArray(0);
  
  _logger->debug(TAG "Vertices updated with window scale %f x %f and texture scale %f x %f", winScaleX, winScaleY, texScaleX, texScaleY);
  return true;
}

GLuint Video::createTexture(unsigned width, unsigned height, retro_pixel_format pixelFormat, bool linear)
{
  GLuint texture;
  const char* format;

  GLint filter = linear ? GL_LINEAR : GL_NEAREST;

  switch (pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    texture = GlUtil::createTexture(width, height, GL_RGBA, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, filter);
    format = "RGBA8888";
    break;

  case RETRO_PIXEL_FORMAT_RGB565:
    texture = GlUtil::createTexture(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, filter);
    format = "RGB565";
    break;

  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    texture = GlUtil::createTexture(width, height, GL_RGB, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, filter);
    format = "RGBA5551";
    break;
  }

  _logger->debug(TAG "Texture created with dimensions %u x %u (%s, %s)", width, height, format, linear ? "linear" : "nearest");
  return texture;
}

bool Video::ensureFramebuffer(unsigned width, unsigned height, retro_pixel_format pixelFormat, bool linearFilter)
{
  if (_texture == 0
    || (_hw.enabled && _hw.frameBuffer == 0)
    || width > _textureWidth || height > _textureHeight
    || pixelFormat != _pixelFormat
    || linearFilter != _linearFilter)
  {
    if (_texture != 0)
    {
      Gl::deleteTextures(1, &_texture);
    }

    _texture = createTexture(width, height, pixelFormat, linearFilter);
    if (_texture == 0)
    {
      _logger->error(TAG " ensure framebuffer: failed to create texture %u x %u (%d)", width, height, pixelFormat);
      return false;
    }

    _textureWidth = width;
    _textureHeight = height;
    _pixelFormat = pixelFormat;
    _linearFilter = linearFilter;

    if (_hw.frameBuffer != 0)
    {
      Gl::deleteRenderbuffers(1, &_hw.renderBuffer);
      Gl::deleteFramebuffers(1, &_hw.frameBuffer);
      _hw.renderBuffer = _hw.frameBuffer = 0;
    }

    if (_hw.enabled)
    {
      _hw.frameBuffer = GlUtil::createFramebuffer(&_hw.renderBuffer, width, height, _texture, _hw.callback->depth, _hw.callback->stencil);
      if (_hw.frameBuffer == 0)
      {
        _logger->error(TAG " ensure framebuffer: failed to create hardware render framebuffer %u x %u", width, height);
        return false;
      }
    }

    // force the view to be updated as well
    _viewWidth = _viewHeight = 0;
  }

  return true;
}

bool Video::ensureView(unsigned width, unsigned height, unsigned windowWidth, unsigned windowHeight, bool preserveAspect, Rotation rotation)
{
  if (_texture == 0)
  {
    _windowWidth = windowWidth;
    _windowHeight = windowHeight;
    return true;
  }

  if (width != _viewWidth || height != _viewHeight
    || windowWidth != _windowWidth || windowHeight != _windowHeight
    || preserveAspect != _preserveAspect
    || rotation != _rotation)
  {
    _preserveAspect = preserveAspect;

    if (rotation != _rotation)
    {
      if (_rotationHandler)
        _rotationHandler(_rotation, rotation);
      _rotation = rotation;
    }

    float texScaleX = (float)width / (float)_textureWidth;
    float texScaleY = (float)height / (float)_textureHeight;

    if (!ensureVertexArray(windowWidth, windowHeight, texScaleX, texScaleY, _posAttribute, _uvAttribute))
    {
      _logger->error(TAG " failed to ensure view: %u x %u", width, height);
      return false;
    }

    _viewWidth = width;
    _viewHeight = height;

    _windowWidth = windowWidth;
    _windowHeight = windowHeight;
  }

  return true;
}

void Video::postHwRenderReset() const
{
  // when hardware rendering, the core may leave behind OpenGL
  // state that interferes with our own software rendering

  Gl::getError();  // clear the error flag, it's not ours

  Gl::disable(GL_SCISSOR_TEST);
  Gl::disable(GL_DEPTH_TEST);
  Gl::disable(GL_CULL_FACE);
  Gl::disable(GL_DITHER);
  Gl::disable(GL_STENCIL_TEST);
  Gl::disable(GL_BLEND);
  Gl::blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  Gl::blendEquation(GL_FUNC_ADD);
  Gl::clearColor(0.0, 0.0, 0.0, 1.0);

  Gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
  Gl::viewport(0, 0, _windowWidth, _windowHeight);
}
