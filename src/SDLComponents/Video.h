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

#pragma once

#include "libretro/Components.h"
#include "Config.h"

#include <SDL_render.h>

class Video: public libretro::VideoComponent
{
public:
  bool init(libretro::LoggerComponent* logger, Config* config, SDL_Renderer* renderer);
  void destroy();

  void draw();

  virtual bool setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, bool needsHardwareRender) override;
  virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) override;

  virtual bool                 supportsContext(enum retro_hw_context_type type) override;
  virtual uintptr_t            getCurrentFramebuffer() override;
  virtual retro_proc_address_t getProcAddress(const char* symbol) override;

  virtual void showMessage(const char* msg, unsigned frames) override;

  const void* getFramebuffer(unsigned* width, unsigned* height, unsigned* pitch, enum retro_pixel_format* format);

protected:
  libretro::LoggerComponent* _logger;
  Config* _config;

  SDL_Renderer*           _renderer;
  SDL_Texture*            _texture;
  bool                    _linearFilter;
  unsigned                _textureWidth;
  unsigned                _textureHeight;
  enum retro_pixel_format _pixelFormat;
  unsigned                _width;
  unsigned                _height;
  float                   _aspect;
};
