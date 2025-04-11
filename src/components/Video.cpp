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
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Video.h"
#include "Gl.h"
#include "GlUtil.h"

#include "BitmapFont.h"

#include "Dialog.h"
#include "jsonsax/jsonsax.h"

#include <SDL_render.h>

#include <math.h>

#define TAG "[VID] "

#define OSD_CHAR_WIDTH 8
#define OSD_CHAR_HEIGHT 16
#define OSD_PADDING 4
#define OSD_SPEED_INDICATOR_WIDTH 32
#define OSD_SPEED_INDICATOR_HEIGHT 32
#define OSD_SPEED_INDICATOR_PADDING 6

struct VertexData
{
  float x, y, u, v;
};

Video::Video()
{
  _enabled = true;

  _pixelFormat = RETRO_PIXEL_FORMAT_UNKNOWN;
  _windowWidth = _windowHeight = 0;
  _textureWidth = _textureHeight = 0;
  _viewWidth = _viewHeight = 0;

  _vertexArray = _vertexBuffer = 0;
  _texture = _speedIndicatorTexture = 0;
  _rotation = Rotation::None;
  _rotationHandler = NULL;

  memset(_messageTexture, 0, sizeof(_messageTexture));
  memset(_messageWidth, 0, sizeof(_messageWidth));
  memset(_messageFrames, 0, sizeof(_messageFrames));
  _numMessages = 0;

  _preserveAspect = true;
  _linearFilter = false;

  _hw.enabled = false;
  _hw.frameBuffer = _hw.renderBuffer = 0;
  _hw.callback = nullptr;
}

bool Video::init(libretro::LoggerComponent* logger, libretro::VideoContextComponent *ctx, Config* config)
{
  _logger = logger;
  _ctx = ctx;
  _config = config;

  // NOTE: Video::init is called after Video::deserializeSettings. Make sure not to overwrite anyting
  // stored in the .cfg file.

  _program = createProgram(&_posAttribute, &_uvAttribute, &_texUniform);

  if (!Gl::ok())
  {
    destroy();
    return false;
  }

  const VertexData vertexData[] = {
    {-1.0f, -1.0f,  0.0f,  1.0f},
    {-1.0f,  1.0f,  0.0f,  0.0f},
    { 1.0f, -1.0f,  1.0f,  1.0f},
    { 1.0f,  1.0f,  1.0f,  0.0f}
  };

  Gl::genBuffers(1, &_indentityVertexBuffer);
  Gl::bindBuffer(GL_ARRAY_BUFFER, _indentityVertexBuffer);
  Gl::bufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
  Gl::bindBuffer(GL_ARRAY_BUFFER, 0);

  return true;
}

void Video::destroy()
{
  _ctx->enableCoreContext(false);
  if (_texture != 0)
  {
    Gl::deleteTextures(1, &_texture);
    _texture = 0;
  }

  for (unsigned i = 0; i < _numMessages; ++i)
  {
    if (_messageTexture[i] != 0)
    {
      Gl::deleteTextures(1, &_messageTexture[i]);
      _messageTexture[i] = 0;
    }
  }
  _numMessages = 0;

  if (_speedIndicatorTexture != 0)
  {
    Gl::deleteTextures(1, &_speedIndicatorTexture);
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

  if (_indentityVertexBuffer != 0)
  {
    Gl::deleteBuffers(1, &_indentityVertexBuffer);
    _indentityVertexBuffer = 0;
  }

  if (_program != 0)
  {
    Gl::deleteProgram(_program);
    _program = 0;
  }

  _ctx->enableCoreContext(true);
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

void Video::setEnabled(bool enabled)
{
  _enabled = enabled;
}

void Video::clear()
{
  if (_hw.frameBuffer != 0)
  {
    Gl::bindFramebuffer(GL_FRAMEBUFFER, _hw.frameBuffer);
    Gl::clearColor(0.0, 0.0, 0.0, 1.0);
    Gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  _ctx->enableCoreContext(false);
  Gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
  Gl::clearColor(0.0, 0.0, 0.0, 1.0);
  Gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  _ctx->enableCoreContext(true);
  _ctx->swapBuffers();
}

void Video::redraw()
{
  _ctx->enableCoreContext(false);
  draw(true);
  _ctx->enableCoreContext(true);
}

void Video::draw(bool force)
{
  if (_texture != 0 && (force || _enabled))
  {
    Gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
    Gl::viewport(0, 0, _windowWidth, _windowHeight);
    Gl::clearColor(0.0, 0.0, 0.0, 1.0);
    Gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Gl::useProgram(_program);

    Gl::bindVertexArray(_vertexArray);

    Gl::activeTexture(GL_TEXTURE0);
    Gl::bindTexture(GL_TEXTURE_2D, _texture);
    Gl::uniform1i(_texUniform, 0);

    Gl::drawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (_numMessages != 0 || _speedIndicatorTexture != 0)
    {
      Gl::bindBuffer(GL_ARRAY_BUFFER, _indentityVertexBuffer);
      Gl::enableVertexAttribArray(_posAttribute);
      Gl::vertexAttribPointer(_posAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, x));
      Gl::enableVertexAttribArray(_uvAttribute);
      Gl::vertexAttribPointer(_uvAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, u));
      Gl::bindBuffer(GL_ARRAY_BUFFER, 0);

      if (_speedIndicatorTexture != 0)
      {
        Gl::viewport(_windowWidth - 8 - OSD_SPEED_INDICATOR_WIDTH, _windowHeight - 8 - OSD_SPEED_INDICATOR_HEIGHT,
          OSD_SPEED_INDICATOR_WIDTH, OSD_SPEED_INDICATOR_WIDTH);
        Gl::bindTexture(GL_TEXTURE_2D, _speedIndicatorTexture);
        Gl::uniform1i(_texUniform, 0);
        Gl::drawArrays(GL_TRIANGLE_STRIP, 0, 4);
      }

      const GLint messageHeight = (OSD_PADDING + OSD_CHAR_HEIGHT + OSD_PADDING);
      const GLint x = 8;
      GLint y = 8;

      for (int i = _numMessages - 1; i >= 0; --i)
      {
        Gl::viewport(x, y, _messageWidth[i], messageHeight);
        Gl::bindTexture(GL_TEXTURE_2D, _messageTexture[i]);
        Gl::uniform1i(_texUniform, 0);
        Gl::drawArrays(GL_TRIANGLE_STRIP, 0, 4);

        y += messageHeight + 4;

        if (--_messageFrames[i] == 0)
        {
          Gl::deleteTextures(1, &_messageTexture[i]);

          --_numMessages;
          for (int j = i; j < (int)_numMessages; ++j)
          {
            _messageTexture[j] = _messageTexture[j + 1];
            _messageWidth[j] = _messageWidth[j + 1];
            _messageFrames[j] = _messageFrames[j + 1];
          }

          _messageTexture[_numMessages] = 0;
          _messageWidth[_numMessages] = 0;
          _messageFrames[_numMessages] = 0;
        }
      }

      Gl::bindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
      Gl::enableVertexAttribArray(_posAttribute);
      Gl::vertexAttribPointer(_posAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, x));
      Gl::enableVertexAttribArray(_uvAttribute);
      Gl::vertexAttribPointer(_uvAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, u));
      Gl::bindBuffer(GL_ARRAY_BUFFER, 0);
    }

    Gl::bindTexture(GL_TEXTURE_2D, 0);
    Gl::bindVertexArray(0);
    Gl::useProgram(0);

    _ctx->swapBuffers();
  }
}

bool Video::setGeometry(unsigned width, unsigned height, unsigned maxWidth, unsigned maxHeight, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback)
{
  bool hardwareRender = hwRenderCallback != nullptr;

  if (hardwareRender)
  {
    if (Gl::getVersion() < 300)
    {
      MessageBox(NULL, "OpenGL 3.0 required for hardware rendering", "Initialization failed", MB_OK | MB_ICONERROR);
      return false;
    }
  }

  _hw.enabled = hardwareRender;
  _hw.callback = hwRenderCallback;

  if (!ensureFramebuffer(std::max(width, maxWidth), std::max(height, maxHeight), pixelFormat, _linearFilter))
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
    _ctx->enableCoreContext(false);
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
    draw();
    _ctx->enableCoreContext(true);
  }
  else if (_hw.enabled && data == RETRO_HW_FRAME_BUFFER_VALID)
  {
    clearErrors();
    _ctx->enableCoreContext(false);
    ensureView(width, height, _windowWidth, _windowHeight, _preserveAspect, _rotation);
    draw();
    _ctx->enableCoreContext(true);
  }
}

void Video::reset() {
  if (_hw.enabled) {
    if (_hw.frameBuffer != 0)
    {
      Gl::deleteRenderbuffers(1, &_hw.renderBuffer);
      Gl::deleteFramebuffers(1, &_hw.frameBuffer);
      _hw.renderBuffer = _hw.frameBuffer = 0;
    }
    _ctx->resetCoreContext();
  }
}

bool Video::supportsContext(enum retro_hw_context_type type)
{
  switch (type)
  {
    case RETRO_HW_CONTEXT_OPENGL:
    case RETRO_HW_CONTEXT_OPENGL_CORE:
      return true;

    default:
      return false;
  }
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

  if (frames == 0)
    return;

  if (_numMessages == sizeof(_messageTexture) / sizeof(_messageTexture[0]))
  {
    /* list full, discard oldest */
    Gl::deleteTextures(1, &_messageTexture[0]);

    --_numMessages;
    for (unsigned i = 0; i < _numMessages; ++i)
    {
      _messageTexture[i] = _messageTexture[i + 1];
      _messageWidth[i] = _messageWidth[i + 1];
      _messageFrames[i] = _messageFrames[i + 1];
    }
  }

  const size_t len = strlen(msg);
  const size_t messageHeight = OSD_PADDING + OSD_CHAR_HEIGHT + OSD_PADDING;
  const size_t messageWidth = OSD_PADDING + len * OSD_CHAR_WIDTH + OSD_PADDING;
  const size_t numBytes = messageWidth * messageHeight * 2;

  uint16_t* data = (uint16_t*)malloc(numBytes);
  if (!data)
  {
    _numMessages--;
    return;
  }

  memset(data, 0, numBytes);

  uint16_t* start = &data[messageWidth * OSD_PADDING + OSD_PADDING];
  for (size_t i = 0; i < len; i++)
  {
    uint16_t* out = &start[i * OSD_CHAR_WIDTH];
    char c = msg[i];
    if (c < 0x20 || c > 0x7F)
      c = 0x80;
    uint8_t* in = &_bitmapFont[(c - 0x20) * 16];
    for (int j = 0; j < OSD_CHAR_HEIGHT; j++)
    {
      uint8_t pixels = *in++;

      out[7] = 0xFFFF * (pixels & 0x01); pixels >>= 1;
      out[6] = 0xFFFF * (pixels & 0x01); pixels >>= 1;
      out[5] = 0xFFFF * (pixels & 0x01); pixels >>= 1;
      out[4] = 0xFFFF * (pixels & 0x01); pixels >>= 1;
      out[3] = 0xFFFF * (pixels & 0x01); pixels >>= 1;
      out[2] = 0xFFFF * (pixels & 0x01); pixels >>= 1;
      out[1] = 0xFFFF * (pixels & 0x01); pixels >>= 1;
      out[0] = 0xFFFF * (pixels & 0x01);

      out += messageWidth;
    }
  }

  _ctx->enableCoreContext(false);

  const GLuint texture = GlUtil::createTexture(messageWidth, messageHeight, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, GL_NEAREST);
  if (texture)
  {
    _messageTexture[_numMessages] = texture;
    _messageFrames[_numMessages] = frames;
    _messageWidth[_numMessages] = messageWidth;
    ++_numMessages;

    Gl::bindTexture(GL_TEXTURE_2D, texture);
    Gl::pixelStorei(GL_UNPACK_ROW_LENGTH, messageWidth);
    Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, messageWidth, messageHeight, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);

    Gl::pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    Gl::bindTexture(GL_TEXTURE_2D, 0);
  }

  _ctx->enableCoreContext(true);

  free(data);
}

void Video::showSpeedIndicator(Speed indicator)
{
  if (indicator == Speed::None)
  {
    if (_speedIndicatorTexture != 0)
    {
      Gl::deleteTextures(1, &_speedIndicatorTexture);
      _speedIndicatorTexture = 0;
    }

    return;
  }

  const size_t numBytes = OSD_SPEED_INDICATOR_WIDTH * OSD_SPEED_INDICATOR_HEIGHT * 2;

  uint16_t* data = (uint16_t*)malloc(numBytes);
  if (!data)
  {
    showSpeedIndicator(Speed::None);
    return;
  }

  if (_speedIndicatorTexture == 0)
  {
    _speedIndicatorTexture = GlUtil::createTexture(OSD_SPEED_INDICATOR_WIDTH, OSD_SPEED_INDICATOR_HEIGHT, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, GL_NEAREST);
    if (_speedIndicatorTexture == 0)
    {
      free(data);
      return;
    }
  }

  memset(data, 0, numBytes);
  uint16_t* out = &data[OSD_SPEED_INDICATOR_PADDING * OSD_SPEED_INDICATOR_WIDTH + OSD_SPEED_INDICATOR_PADDING];
  const size_t width = OSD_SPEED_INDICATOR_WIDTH - OSD_SPEED_INDICATOR_PADDING * 2;
  const size_t height = OSD_SPEED_INDICATOR_HEIGHT - OSD_SPEED_INDICATOR_PADDING * 2;

  switch (indicator)
  {
    case Speed::Paused:
      for (size_t i = 0; i < (width + 2) / 3; ++i)
      {
        out[i] = 0xFFFF;
        out[width - i - 1] = 0xFFFF;
      }
      for (size_t i = 1; i < height; ++i)
        memcpy(&out[i * OSD_SPEED_INDICATOR_WIDTH], out, width * 2);
      break;

    case Speed::FastForwarding:
      for (size_t i = 0; i < height / 2; ++i)
      {
        for (size_t j = 0; j <= i; ++j)
        {
          out[j] = 0xFFFF;
          out[j + width / 2 + 1] = 0xFFFF;
        }
        out += OSD_SPEED_INDICATOR_WIDTH;
      }
      for (size_t i = 0; i < height / 2; ++i)
      {
        for (size_t j = 0; j < height / 2 - i; ++j)
        {
          out[j] = 0xFFFF;
          out[j + width / 2 + 1] = 0xFFFF;
        }
        out += OSD_SPEED_INDICATOR_WIDTH;
      }
      break;

    default:
      break;
  }

  Gl::bindTexture(GL_TEXTURE_2D, _speedIndicatorTexture);
  Gl::pixelStorei(GL_UNPACK_ROW_LENGTH, OSD_SPEED_INDICATOR_WIDTH);
  Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, OSD_SPEED_INDICATOR_WIDTH, OSD_SPEED_INDICATOR_HEIGHT, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
  Gl::pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  Gl::bindTexture(GL_TEXTURE_2D, 0);

  free(data);
}

void Video::windowResized(unsigned width, unsigned height)
{
  _ctx->enableCoreContext(false);
  Gl::viewport(0, 0, width, height);
  ensureView(_viewWidth, _viewHeight, width, height, _preserveAspect, _rotation);
  draw(true);
  _logger->debug(TAG "Window resized to %u x %u", width, height);
  _ctx->enableCoreContext(true);
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

  _ctx->enableCoreContext(false);
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
  _ctx->enableCoreContext(true);

  if (_hw.enabled && _hw.callback->bottom_left_origin)
    verticalFlipRawTexture(pixels, _viewHeight, _textureWidth * bpp);

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

  _ctx->enableCoreContext(false);
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

  draw(true);
  _ctx->enableCoreContext(true);
}

std::string Video::serializeSettings()
{
  std::string json("{");

  json.append("\"preserveAspect\":");
  json.append(_preserveAspect ? "true" : "false");
  json.append(",");

  json.append("\"linearFilter\":");
  json.append(_linearFilter ? "true" : "false");

  json.append("}");
  return json;
}

bool Video::deserializeSettings(const char* json)
{
  struct Deserialize
  {
    Video* self;
    std::string key;
  };

  Deserialize ud;
  ud.self = this;

  jsonsax_result_t res = jsonsax_parse(json, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
  {
    auto ud = (Deserialize*)udata;

    if (event == JSONSAX_KEY)
    {
      ud->key = std::string(str, num);
    }
    else if (event == JSONSAX_BOOLEAN)
    {
      if (ud->key == "preserveAspect" || ud->key == "_preserveAspect")
      {
        ud->self->_preserveAspect = num != 0;
      }
      if (ud->key == "linearFilter" || ud->key == "_linearFilter")
      {
        ud->self->_linearFilter = num != 0;
      }
    }

    return 0;
  });

  return (res == JSONSAX_OK);
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
  float winScaleX, winScaleY;

  _ctx->enableCoreContext(false);
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

    _viewScaledWidth = w;
    _viewScaledHeight = h;
    winScaleX = (float)w / (float)windowWidth;
    winScaleY = (float)h / (float)windowHeight;
  }
  else
  {
    winScaleX = winScaleY = 1.0f;
    _viewScaledWidth = windowWidth;
    _viewScaledHeight = windowHeight;
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
  {
    Gl::deleteVertexArrays(1, &_vertexArray);
    _vertexArray = 0;
  }

  if (_hw.enabled)
  {
    Gl::genVertexArray(1, &_vertexArray);
    if (_vertexArray == 0)
      return false;
    Gl::bindVertexArray(_vertexArray);
  }

  if (_vertexBuffer != 0)
    Gl::deleteBuffers(1, &_vertexBuffer);

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
  Gl::bindBuffer(GL_ARRAY_BUFFER, 0);
  
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
  const bool hwEnabled = (_hw.frameBuffer != 0);
  _ctx->enableCoreContext(true);
  if (_texture == 0
    || (_hw.enabled != hwEnabled)
    || width > _textureWidth || height > _textureHeight
    || pixelFormat != _pixelFormat
    || linearFilter != _linearFilter)
  {
    if (_texture != 0)
      Gl::deleteTextures(1, &_texture);

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

void Video::clearErrors() {
  for (GLenum error = Gl::getError(); error != GL_NO_ERROR; error = Gl::getError())
  {
    _logger->error(TAG "Unhandled OpenGL error from core: %s", Gl::getErrorMsg(error));
  }
}
