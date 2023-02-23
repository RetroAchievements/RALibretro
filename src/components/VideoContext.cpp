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

#include "VideoContext.h"

#define TAG "[CONTEXT] "

bool VideoContext::init(libretro::LoggerComponent* logger, SDL_Window* window)
{
  _logger = logger;
  _window = window;

  _raContext = SDL_GL_CreateContext(_window);
  _coreContext = SDL_GL_CreateContext(_window);
  if (SDL_GL_MakeCurrent(_window, _raContext) != 0)
  {
    _logger->error(TAG "SDL_GL_MakeCurrent: %s", SDL_GetError());
    return false;
  }

  return true;
}

void VideoContext::destroy()
{
}

void VideoContext::enableCoreContext(bool enable)
{
  if (enable)
  {
    SDL_GL_MakeCurrent(_window, _coreContext);
  }
  else
  {
    SDL_GL_MakeCurrent(_window, _raContext);
  }
}

void VideoContext::swapBuffers()
{
  SDL_GL_SwapWindow(_window);
}
