/*
Copyright (C) 2023 Brian Weiss

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

#pragma once

#include "libretro/Components.h"

#include "libretro/libretro.h"
#include "Audio.h"

class Microphone : public libretro::MicrophoneComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy();

  virtual retro_microphone_t* openMic(const retro_microphone_params_t* params) override;
  virtual void closeMic(retro_microphone_t* microphone) override;
  virtual bool getMicParams(const retro_microphone_t* microphone, retro_microphone_params_t* params) override;
  virtual bool getMicState(const retro_microphone_t* microphone) override;
  virtual bool setMicState(retro_microphone_t* microphone, bool state) override;
  virtual int readMic(retro_microphone_t* microphone, int16_t* frames, size_t num_frames) override;

protected:
  libretro::LoggerComponent* _logger;

  Fifo* _fifo;

  struct SDLData;
  struct SDLData* _data;

  static void recordCallback(void* data, Uint8* stream, int len);
};
