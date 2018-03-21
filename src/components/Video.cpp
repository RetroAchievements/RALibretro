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

#include <SDL_hints.h>
#include <math.h>

bool Video::init(libretro::LoggerComponent* logger, Config* config, SDL_Renderer* renderer)
{
  _logger = logger;
  _config = config;

  _inited = false;

  _renderer = renderer;
  _texture = NULL;
  _linearFilter = false;
  _width = _height = 0;
  return true;
}

void Video::destroy()
{
  /*if (_texture != NULL)
  {
    SDL_DestroyTexture(_texture);
    _texture = NULL;
  }*/
}

void Video::draw()
{
#if 0
  if (_texture != NULL)
  {
    bool linearFilter = _config->linearFilter();

    if (linearFilter != _linearFilter)
    {
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linearFilter ? "linear" : "nearest");
      _linearFilter = linearFilter;

      setGeometry(_width, _height, _aspect, _pixelFormat, false);
    }

    int w, h;

    if (SDL_GetRendererOutputSize(_renderer, &w, &h) != 0)
    {
      _logger->printf(RETRO_LOG_ERROR, "SDL_GetRendererOutputSize: %s", SDL_GetError());
      return;
    }

    float height = h;
    float width = height * _aspect;

    if (width > w)
    {
      width = w;
      height = width / _aspect;
    }

    SDL_Rect src;
    src.x = src.y = 0;
    src.w = _width;
    src.h = _height;

    int res;

    if (_config->preserveAspect())
    {
      SDL_Rect dst;
      dst.w = ceil(width);
      dst.h = ceil(height);
      dst.x = (w - dst.w) / 2;
      dst.y = (h - dst.h) / 2;

      res = SDL_RenderCopy(_renderer, _texture, &src, &dst);
    }
    else
    {
      res = SDL_RenderCopy(_renderer, _texture, &src, NULL);
    }

    if (res != 0)
    {
      _logger->printf(RETRO_LOG_ERROR, "SDL_RenderCopy: %s", SDL_GetError());
      return;
    }
  }
#endif
}

bool Video::setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, bool needsHardwareRender)
{
#if 0
  if (_inited)
  {
    // Destroy everything
  }

  Gl gl(_logger);

  GLuint _framebuffer;
  gl.genFramebuffers(1, &_framebuffer);
  gl.bindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

  GLuint _texture;
  gl.genTextures(1, &_texture);
  gl.bindTexture(GL_TEXTURE_2D, _texture);

  gl.texImage2D(GL_TEXTURE_2D, 0,GL_RGB, 1024, 768, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);

  gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  gl.framebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _texture, 0);

  GLuint _depth;
  gl.genRenderbuffers(1, &_depth);
  gl.bindRenderbuffer(GL_RENDERBUFFER, _depth);
  gl.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
  gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

  GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
  gl.drawBuffers(1, drawBuffers);







  // TODO implement an OpenGL renderer
  (void)needsHardwareRender;

  if (_texture != NULL)
  {
    SDL_DestroyTexture(_texture);
  }

  unsigned format;

  switch (pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    format = SDL_PIXELFORMAT_ARGB8888;
    break;
    
  case RETRO_PIXEL_FORMAT_RGB565:
    format = SDL_PIXELFORMAT_RGB565;
    break;
    
  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    format = SDL_PIXELFORMAT_ARGB1555;
    break;
  }

  _texture = SDL_CreateTexture(_renderer, format, SDL_TEXTUREACCESS_STREAMING, width, height);

  if (_texture == NULL)
  {
    _logger->printf(RETRO_LOG_ERROR, "SDL_CreateTexture: %s", SDL_GetError());
    return false;
  }

  if (SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_NONE) != 0)
  {
    _logger->printf(RETRO_LOG_ERROR, "SDL_SetTextureBlendMode: %s", SDL_GetError());
    return false;
  }

  _textureWidth = width;
  _textureHeight = height;
  _pixelFormat = pixelFormat;
  _aspect = aspect;
#endif

  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
#if 0
  if (data != NULL && data != RETRO_HW_FRAME_BUFFER_VALID)
  {
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;

    uint8_t* target_pixels;
    int target_pitch;

    if (SDL_LockTexture(_texture, &rect, (void**)&target_pixels, &target_pitch) != 0)
    {
      _logger->printf(RETRO_LOG_ERROR, "SDL_LockTexture: %s", SDL_GetError());
      return;
    }

    auto source_pixels = (uint8_t*)data;
    size_t bpp = _pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
    size_t bytes = width * bpp;

    if (bytes > (unsigned)target_pitch)
    {
      bytes = target_pitch;
    }

    for (unsigned y = 0; y < height; y++)
    {
      memcpy(target_pixels, source_pixels, bytes);
      source_pixels += pitch;
      target_pixels += target_pitch;
    }

    SDL_UnlockTexture(_texture);

    _width = width;
    _height = height;
  }
#endif
}

bool Video::supportsContext(enum retro_hw_context_type type)
{
  // TODO Do we really support all those types?

  switch (type)
  {
  case RETRO_HW_CONTEXT_OPENGL:
  case RETRO_HW_CONTEXT_OPENGLES2:
  case RETRO_HW_CONTEXT_OPENGL_CORE:
  case RETRO_HW_CONTEXT_OPENGLES3:
  case RETRO_HW_CONTEXT_OPENGLES_VERSION:
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

const void* Video::getFramebuffer(unsigned* width, unsigned* height, unsigned* pitch, enum retro_pixel_format* format)
{
  unsigned bpp = _pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
  void* result = malloc(_width * _height * bpp);

  if (result == NULL)
  {
    return NULL;
  }

  uint8_t* target_pixels = (uint8_t*)result;
  int target_pitch = _width * bpp;

  SDL_Rect rect;
  rect.x = 0;
  rect.y = 0;
  rect.w = _width;
  rect.h = _height;

  uint8_t* source_pixels;
  int source_pitch;

  if (SDL_LockTexture(_texture, &rect, (void**)&source_pixels, &source_pitch) != 0)
  {
    _logger->printf(RETRO_LOG_ERROR, "SDL_LockTexture: %s", SDL_GetError());
    return NULL;
  }

  size_t bytes = target_pitch;

  if (bytes > (unsigned)source_pitch)
  {
    bytes = source_pitch;
  }

  for (unsigned y = 0; y < _height; y++)
  {
    memcpy(target_pixels, source_pixels, bytes);
    source_pixels += source_pitch;
    target_pixels += target_pitch;
  }

  SDL_UnlockTexture(_texture);

  *width = _width;
  *height = _height;
  *pitch = target_pitch;
  *format = _pixelFormat;
  return result;
}