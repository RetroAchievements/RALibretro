#pragma once

#include "libretro/Components.h"

#include <SDL_render.h>

class Video: public libretro::VideoComponent
{
public:
  bool init(libretro::LoggerComponent* logger, SDL_Renderer* renderer);
  void destroy();

  void draw();

  virtual bool setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, bool needsHardwareRender) override;
  virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) override;

  virtual bool                 supportsContext(enum retro_hw_context_type type) override;
  virtual uintptr_t            getCurrentFramebuffer() override;
  virtual retro_proc_address_t getProcAddress(const char* symbol) override;

  virtual void showMessage(const char* msg, unsigned frames) override;

protected:
  libretro::LoggerComponent* _logger;

  SDL_Renderer*           _renderer;
  SDL_Texture*            _texture;
  unsigned                _textureWidth;
  unsigned                _textureHeight;
  enum retro_pixel_format _pixelFormat;
  unsigned                _width;
  unsigned                _height;
  float                   _aspect;
};
