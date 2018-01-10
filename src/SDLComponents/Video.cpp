#include "Video.h"

#include <SDL_Hints.h>

bool Video::init(libretro::LoggerComponent* logger, Config* config, SDL_Renderer* renderer)
{
  _logger = logger;
  _config = config;
  _renderer = renderer;
  _texture = NULL;
  _linearFilter = false;
  _width = _height = 0;
  return true;
}

void Video::destroy()
{
  if (_texture != NULL)
  {
    SDL_DestroyTexture(_texture);
    _texture = NULL;
  }
}

void Video::draw()
{
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
}

bool Video::setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, bool needsHardwareRender)
{
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

  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
  if (data != NULL && data != RETRO_HW_FRAME_BUFFER_VALID)
  {
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;

    if (SDL_UpdateTexture(_texture, &rect, data, pitch) != 0)
    {
      _logger->printf(RETRO_LOG_ERROR, "SDL_UpdateTexture: %s", SDL_GetError());
      return;
    }

    _width = width;
    _height = height;
  }
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
  return (retro_proc_address_t)SDL_GL_GetProcAddress(symbol);
}

void Video::showMessage(const char* msg, unsigned frames)
{
  _logger->printf(RETRO_LOG_INFO, "OSD message (%u): %s", frames, msg);
}
