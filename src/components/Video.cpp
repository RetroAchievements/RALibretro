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

#include <SDL_render.h>

#include <math.h>

bool Video::init(libretro::LoggerComponent* logger, Config* config)
{
  _logger = logger;
  _config = config;

  _linearFilter = false;
  _windowWidth = _windowHeight = 0;
  _textureWidth = _textureHeight = 0;
  _viewWidth = _viewHeight = 0;

  _program = 0;
  _posAttribute = -1;
  _uvAttribute = -1;
  _ratioUniform = -1;
  _texUniform = -1;
  _vertexBuffer = 0;
  _texture = 0;
  _framebuffer = 0;
  _renderbuffer = 0;

  createProgram();
  createVertexAttributes();

  if (!Gl::ok())
  {
    destroy();
    return false;
  }

  return true;
}

void Video::destroy()
{
  /*if (_renderbuffer != 0)
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
  }*/
}

void Video::draw()
{
  if (_texture != 0)
  {
    bool linearFilter = _config->linearFilter();

    if (linearFilter != _linearFilter)
    {
      _linearFilter = linearFilter;
      GLint filter = linearFilter ? GL_LINEAR : GL_NEAREST;
      Gl::texParameteri(_texture, GL_TEXTURE_MIN_FILTER, filter);
      Gl::texParameteri(_texture, GL_TEXTURE_MAG_FILTER, filter);
    }

    float height = (float)_windowHeight;
    float width = height * _aspect;

    if (width > _windowWidth)
    {
      width = (float)_windowWidth;
      height = width / _aspect;
    }

    float sx = 1.0f;
    float sy = 1.0f;

    if (_config->preserveAspect())
    {
      sx = (float)_viewWidth / (float)_windowWidth;
      sy = (float)_viewHeight / (float)_windowHeight;
    }

    /*Gl::clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Gl::clear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);*/
    
    Gl::useProgram(_program);

    Gl::enableVertexAttribArray(_posAttribute);
    Gl::enableVertexAttribArray(_uvAttribute);

    Gl::activeTexture(GL_TEXTURE0);
    Gl::bindTexture(GL_TEXTURE_2D, _texture);
    Gl::uniform1i(_texUniform, 0);

    Gl::uniform2f(_ratioUniform, sx, sy);

    Gl::drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
}

bool Video::setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback)
{
  destroy();

  Gl::genTextures(1, &_texture);

  if (!Gl::ok())
  {
    return false;
  }

  Gl::bindTexture(GL_TEXTURE_2D, _texture);

  switch (pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    Gl::texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    break;
    
  case RETRO_PIXEL_FORMAT_RGB565:
    Gl::texImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    break;
    
  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    Gl::texImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
    break;
  }

  _textureWidth = width;
  _textureHeight = height;
  _aspect = aspect;
  _pixelFormat = pixelFormat;

  _logger->printf(RETRO_LOG_DEBUG, "Geometry set to %u x %u (1:%f)", width, height, aspect);

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
  }

  if (!Gl::ok())
  {
    destroy();
    return false;
  }

  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
  if (data != NULL && data != RETRO_HW_FRAME_BUFFER_VALID)
  {
    Gl::bindTexture(GL_TEXTURE_2D, _texture);
    uint8_t* p = (uint8_t*)data;

    switch (_pixelFormat)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      {
        uint32_t* q = (uint32_t*)alloca(width * 4);

        if (q != NULL)
        {
          for (unsigned y = 0; y < height; y++)
          {
            uint32_t* r = q;
            uint32_t* s = (uint32_t*)p;

            for (unsigned x = 0; x < width; x++)
            {
              uint32_t color = *s++;
              uint32_t red   = (color >> 16) & 255;
              uint32_t green = (color >> 8) & 255;
              uint32_t blue  = color & 255;
              *r++ = 0xff000000UL | blue << 16 | green << 8 | red;
            }

            Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void*)q);
            p += pitch;
          }
        }
      }
      
      break;
      
    case RETRO_PIXEL_FORMAT_RGB565:
      for (unsigned y = 0; y < height; y++)
      {
        Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, (void*)p);
        p += pitch;
      }

      break;
      
    case RETRO_PIXEL_FORMAT_0RGB1555:
    default:
      for (unsigned y = 0; y < height; y++)
      {
        Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGB, GL_UNSIGNED_SHORT_1_5_5_5_REV, (void*)p);
        p += pitch;
      }

      break;
    }

    if (width != _viewWidth || height != _viewHeight)
    {
      _viewWidth = width;
      _viewHeight = height;

      _logger->printf(RETRO_LOG_DEBUG, "Video refreshed with geometry %u x %u", width, height);
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
}

const void* Video::getFramebuffer(unsigned* width, unsigned* height, unsigned* pitch, enum retro_pixel_format* format)
{
#if 0
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
#endif
  return NULL;
}

void Video::createProgram()
{
  static const char* s_vertexShader =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "uniform vec2 u_ratio;\n"
    "void main() {\n"
    "  v_uv = a_uv;\n"
    "  gl_Position = vec4(a_pos * u_ratio, 0.0, 1.0);\n"
    "}";
  
  static const char* s_fragmentShader =
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_tex, v_uv);\n"
    "}";
  
  _program = Gl::createProgram(s_vertexShader, s_fragmentShader);

  _posAttribute = Gl::getAttribLocation(_program, "a_pos");
  _uvAttribute = Gl::getAttribLocation(_program, "a_uv");
  _ratioUniform = Gl::getUniformLocation(_program, "u_ratio");
  _texUniform = Gl::getUniformLocation(_program, "u_tex");
}

void Video::createVertexAttributes()
{
  struct VertexData
  {
    float x, y, u, v;
  };

  static const VertexData vertexData[] = {
    {-1.0f, -1.0f, 0.0f, 0.0f},
    {-1.0f,  1.0f, 0.0f, 1.0f},
    { 1.0f, -1.0f, 1.0f, 0.0f},
    { 1.0f,  1.0f, 1.0f, 1.0f}
  };
  
  Gl::genBuffers(1, &_vertexBuffer);
  Gl::bindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  Gl::bufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
  
  Gl::vertexAttribPointer(_posAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, x));
  Gl::vertexAttribPointer(_uvAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (const GLvoid*)offsetof(VertexData, u));
}
