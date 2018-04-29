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

#include "Dialog.h"
#include "Util.h"
#include "jsonsax/jsonsax.h"
#include "BoxyBold.h"

#include <SDL_render.h>

#include <math.h>

bool Video::init(libretro::LoggerComponent* logger, Config* config)
{
  _logger = logger;
  _config = config;

  _pixelFormat = RETRO_PIXEL_FORMAT_UNKNOWN;
  _windowWidth = _windowHeight = 0;
  _viewWidth = 0;

  _preserveAspect = false;
  _linearFilter = false;

  _blitter.init();
  _quad.init();
  //_osdProgram = createOsdProgram(&_osdPosAttribute, &_osdUvAttribute, &_osdTexUniform, &_osdTimeUniform);

  if (!Gl::ok())
  {
    destroy();
    return false;
  }

  return true;
}

void Video::destroy()
{
  _texture.destroy();
  _quad.destroy();
  _blitter.destroy();
}

void Video::draw()
{
  _blitter.run(&_quad, &_texture);
}

bool Video::setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback)
{
  (void)hwRenderCallback;

  const char* format = "?";

  switch (pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888: format = "XRGB8888"; break;
  case RETRO_PIXEL_FORMAT_RGB565:   format = "RGB565"; break;
  case RETRO_PIXEL_FORMAT_0RGB1555: format = "0RGB1555"; break;
  case RETRO_PIXEL_FORMAT_UNKNOWN:  format = "unknown"; break;
  }

  _logger->printf(RETRO_LOG_DEBUG, "Geometry set to %u x %u, %s, 1:%f", width, height, format, aspect);

  _aspect = aspect;
  _pixelFormat = pixelFormat;

  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
  if (data != NULL && data != RETRO_HW_FRAME_BUFFER_VALID)
  {
    bool updateVb = false;
    unsigned textureWidth = pitch;
    GLenum format, type;

    switch (_pixelFormat)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      textureWidth /= 4;
      format = GL_BGRA;
      type = GL_UNSIGNED_INT_8_8_8_8_REV;
      break;
    
    case RETRO_PIXEL_FORMAT_RGB565:
      textureWidth /= 2;
      format = GL_RGB;
      type = GL_UNSIGNED_SHORT_5_6_5;
      break;
    
    case RETRO_PIXEL_FORMAT_0RGB1555: // fallthrough
    default:
      textureWidth /= 2;
      format = GL_RGBA;
      type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
      break;
    }

    if (textureWidth != (unsigned)_texture.getWidth() || height != (unsigned)_texture.getHeight() || width != _viewWidth)
    {
      _texture.destroy();
      _texture.init(textureWidth, height, GL_RGB, _linearFilter);

      _logger->printf(RETRO_LOG_DEBUG, "Texture set to %d x %d, %s", _texture.getWidth(), _texture.getHeight(), _linearFilter ? "linear" : "nearest");

      _viewWidth = width;
      updateVb = true;
    }

    _texture.setData(textureWidth, height, pitch, format, type, data);

    if (updateVb)
    {
      updateVertexBuffer(_windowWidth, _windowHeight, (float)_viewWidth / (float)_texture.getWidth());
    }
  }
}

bool Video::supportsContext(enum retro_hw_context_type type)
{
  (void)type;
  return false;
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

  updateVertexBuffer(width, height, (float)_viewWidth / (float)_texture.getWidth());
  _logger->printf(RETRO_LOG_INFO, "Window resized to %u x %u", width, height);
}

void Video::getFramebufferSize(unsigned* width, unsigned* height, enum retro_pixel_format* format)
{
  unsigned w, h;

  if (_preserveAspect)
  {
    h = _texture.getHeight();
    w = (unsigned)(_texture.getHeight() * _aspect);

    if (w < _viewWidth)
    {
      w = _viewWidth;
      h = (unsigned)(_viewWidth / _aspect);
    }
  }
  else
  {
    w = _viewWidth;
    h = _texture.getHeight();
  }

  *width = w;
  *height = h;
  *format = _pixelFormat;
}

const void* Video::getFramebufferRgba(unsigned* width, unsigned* height, unsigned* pitch)
{
  *width = _viewWidth;
  *height = _texture.getHeight();
  *pitch = _texture.getWidth() * 4;

  return _texture.getData(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
}

void Video::setFramebufferRgb(const void* pixels, unsigned width, unsigned height, unsigned pitch)
{
  const void* converted = util::fromRgb(_logger, pixels, width, height, &pitch, _pixelFormat);

  switch (_pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    _texture.setData(width, height, pitch, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, converted);
    break;

  case RETRO_PIXEL_FORMAT_RGB565:
    _texture.setData(width, height, pitch, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, converted);
    break;

  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    _texture.setData(width, height, pitch, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, converted);
    break;
  }

  free((void*)converted);
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
      _texture.destroy();
      _texture.init(_texture.getWidth(), _texture.getHeight(), GL_RGB, _linearFilter);

      _logger->printf(RETRO_LOG_DEBUG, "Texture set to %d x %d, %s", _texture.getWidth(), _texture.getHeight(), _linearFilter ? "linear" : "nearest");
    }

    if (preserveAspect != _preserveAspect)
    {
      updateVertexBuffer(_windowWidth, _windowHeight, (float)_viewWidth / (float)_texture.getWidth());
    }
  }
}

/*GLuint Video::createOsdProgram(GLint* pos, GLint* uv, GLint* tex, GLint* time)
{
  // Easing tan: f(t) = (tan(t * 3.0 - 1.5) + 14.101419947172) * 0.035457422151326
  // Easing sin: f(t) = sin(t * 3.1415926535898)
  static const char* vertexShader =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "uniform float u_ease;\n"
    "void main() {\n"
    "  float y = tan(u_time * 3.0 - 1.5);\n"
    "  a_pos.y += u_ease * 0.2;\n"
    "  v_uv = a_uv;\n"
    "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}";
  
  static const char* fragmentShader =
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_time;\n"
    "void main() {\n"
    "  vec4 color = texture2D(u_tex, v_uv);\n"
    "  color.a *="
    "  gl_FragColor = texture2D(u_tex, v_uv);\n"
    "}";
  
  GLuint program = GlUtil::createProgram(vertexShader, fragmentShader);

  *pos = Gl::getAttribLocation(program, "a_pos");
  *uv = Gl::getAttribLocation(program, "a_uv");
  *tex = Gl::getUniformLocation(program, "u_tex");

  return program;
}*/

void Video::updateVertexBuffer(unsigned windowWidth, unsigned windowHeight, float texScaleX)
{
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

  _quad.destroy();
  _quad.init(-winScaleX, -winScaleY, winScaleX, winScaleY, 0.0f, 0.0f, texScaleX, 1.0f);
  
  _logger->printf(RETRO_LOG_DEBUG, "Vertices updated with window scale %f x %f and texture scale %f x %f", winScaleX, winScaleY, texScaleX, 1.0f);
}

/*void Video::createOsd(OsdMessage* osd, const char* msg, GLint pos, GLint uv)
{
  static constexpr float scale = 2.0f / 320.0f;

  osd->x = osd->y = osd->t = 0.0f;

  struct VertexData
  {
    float x, y, u, v;
  };

  size_t length = strlen(msg);
  size_t size = sizeof(VertexData) * length * 6;
  VertexData* vd = (VertexData*)malloc(size);
  VertexData* aux = vd;
  unsigned x0i = 0;

  for (size_t i = 0; i < length; i++, msg++)
  {
    int k = *msg;

    if (k >= 32 and k <= 126)
    {
      glyph_t* glyph = g_boxybold + k;

      float x0 = (float)x0i * scale;
      float x1 = ((float)x0i + glyph->w) * scale;
      float y1 = glyph->h * scale;

      float u0 = glyph->x / 128.0f;
      float v0 = glyph->y / 128.0f;
      float u1 = (glyph->x + glyph->w) / 128.0f;
      float v1 = (glyph->t + glyph->h) / 128.0f;

      aux->x = x0; aux->y = y0; aux->u = u0; aux->v = v0; aux++;
      aux->x = x1; aux->y = y1; aux->u = u1; aux->v = v1; aux++;
      aux->x = x0; aux->y = y1; aux->u = u0; aux->v = v1; aux++;
      aux->x = x0; aux->y = y0; aux->u = u0; aux->v = v0; aux++;
      aux->x = x1; aux->y = y0; aux->u = u1; aux->v = v0; aux++;
      aux->x = x1; aux->y = y1; aux->u = u1; aux->v = v1; aux++;

      x0i += aux->dx;
    }
  }

  Gl::genBuffers(1, &osd->vertexBuffer);

  Gl::bindBuffer(GL_ARRAY_BUFFER, osd->vertexBuffer);
  Gl::bufferData(GL_ARRAY_BUFFER, size, vd, GL_STATIC_DRAW);
  
  Gl::vertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, x));
  Gl::vertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, u));
}*/

bool Video::Blitter::init()
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
  
  if (!Program::init(vertexShader, fragmentShader))
  {
    return false;
  }

  _posLocation = getAttribute("a_pos");
  _uvLocation = getAttribute("a_uv");
  _textureLocation = getUniform("u_tex");
  return true;
}

void Video::Blitter::destroy()
{
  Program::destroy();
}

void Video::Blitter::run(const GlUtil::TexturedQuad2D* quad, const GlUtil::Texture* texture)
{
  use();

  texture->setUniform(_textureLocation, 0);

  quad->bind();
  quad->enablePos(_posLocation);
  quad->enableUV(_uvLocation);
  quad->draw();
}
