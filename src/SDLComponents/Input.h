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

#include <map>

#include <SDL_events.h>
#include <SDL_joystick.h>

class Input: public libretro::InputComponent
{
public:
  enum class Button
  {
    kUp,
    kDown,
    kLeft,
    kRight,
    kX,
    kY,
    kA,
    kB,
    kL,
    kR,
    kL2,
    kR2,
    kL3,
    kR3,
    kSelect,
    kStart
  };

  bool init(libretro::LoggerComponent* logger);
  void destroy() {}
  void reset();

  void addController(int which);
  void autoAssign();
  void buttonEvent(Button button, bool pressed);
  void processEvent(const SDL_Event* event);

  virtual void setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) override;

  virtual void     setControllerInfo(const struct retro_controller_info* info, unsigned count) override;
  virtual bool     ctrlUpdated() override;
  virtual unsigned getController(unsigned port) override;

  virtual void    poll() override;
  virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) override;

  std::string serialize();
  void deserialize(const char* json);
  void showDialog();

protected:
  struct Pad
  {
    SDL_JoystickID      _id;
    SDL_GameController* _controller;
    const char*         _controllerName;
    SDL_Joystick*       _joystick;
    const char*         _joystickName;
    uint64_t            _ports;
    int                 _lastDir[6];
    bool                _state[16];
    float               _sensitivity;
  };

  struct Descriptor
  {
    unsigned _port;
    unsigned _device;
    unsigned _index;
    unsigned _button;

    std::string _description;
  };

  struct ControllerInfo
  {
    std::string _description;
    unsigned    _id;
  };

  enum
  {
    kMaxPorts = 8
  };

  void addController(const SDL_Event* event);
  void removeController(const SDL_Event* event);
  void controllerButton(const SDL_Event* event);
  void controllerAxis(const SDL_Event* event);

  static const char* s_getType(int index, void* udata);
  static const char* s_getPad(int index, void* udata);

  libretro::LoggerComponent* _logger;

  bool _updated;

  std::map<SDL_JoystickID, Pad> _pads;
  std::vector<Descriptor>       _descriptors;

  uint64_t                    _ports;
  std::vector<ControllerInfo> _info[kMaxPorts];

  int _devices[kMaxPorts];
};
