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

#pragma once

#include "libretro/Components.h"

#include "Dialog.h"
#include "KeyBinds.h"

#include <map>

#include <SDL_events.h>
#include <SDL_haptic.h>
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
    kStart,
  };

  enum class Axis
  {
    kLeftAxisX,
    kLeftAxisY,
    kRightAxisX,
    kRightAxisY
  };

  enum class MouseButton
  {
    kLeft,
    kRight,
    kMiddle,
  };

  bool init(libretro::LoggerComponent* logger);
  void destroy() {}
  void reset();

  void autoAssign();
  void buttonEvent(int port, Button button, bool pressed);
  void axisEvent(int port, Axis axis, int16_t value);
  void processEvent(const SDL_Event* event, KeyBinds* keyBinds);
  void mouseButtonEvent(MouseButton button, bool pressed);
  void mouseMoveEvent(int relative_x, int relative_y, int absolute_x, int absolute_y);
  void keyboardEvent(enum retro_key key, bool pressed);

  virtual void setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) override;
  virtual void setKeyboardCallback(const struct retro_keyboard_callback* data) override;

  virtual void     setControllerInfo(const struct retro_controller_info* info, unsigned count) override;
  virtual bool     ctrlUpdated() override;
  virtual unsigned getController(unsigned port) override;
  void             getControllerNames(unsigned port, std::vector<std::string>& names, int& selectedIndex) const;
  void             setSelectedControllerIndex(unsigned port, int selectedIndex);

  virtual bool     setRumble(unsigned port, retro_rumble_effect effect, uint16_t strength) override;

  virtual void    poll() override;
  virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) override;
  float getJoystickSensitivity(int joystickId);

  std::string serialize();
  void deserialize(const char* json);

  KeyBinds::Binding captureButtonPress();

protected:
  struct Pad
  {
    SDL_JoystickID      _id;
    SDL_GameController* _controller;
    const char*         _controllerName;
    SDL_Joystick*       _joystick;
    const char*         _joystickName;
    uint64_t            _ports;
    float               _sensitivity;
    unsigned            _navigationPort;
    SDL_Haptic*         _haptic;
    int                 _rumbleEffect;
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
    unsigned    _id = 0;
    int16_t     _state = 0;
    int16_t     _axis[4] = { 0,0,0,0 };
  };

  struct MouseInfo
  {
    int16_t _absolute_x, _absolute_y;
    int16_t _previous_x, _previous_y;
    int16_t _relative_x, _relative_y;
    bool _button[16];
  };

  struct KeyboardInfo
  {
    std::array<bool, RETROK_LAST> _keys;
    struct retro_keyboard_callback _callbacks;
  };

  enum
  {
    kMaxPorts = 8
  };

  SDL_JoystickID addController(int which);
  void addController(const SDL_Event* event, KeyBinds* keyBinds);
  void removeController(const SDL_Event* event);

  static const char* s_getType(int index, void* udata);
  static const char* s_getPad(int index, void* udata);

  libretro::LoggerComponent* _logger;

  bool _updated;

  std::map<SDL_JoystickID, Pad> _pads;
  std::vector<Descriptor>       _descriptors;

  std::map<SDL_JoystickID, SDL_JoystickGUID> _joystickGUIDs;

  uint64_t                    _ports;
  std::vector<ControllerInfo> _info[kMaxPorts];
  MouseInfo                   _mouse;
  KeyboardInfo                _keyboard;

  int _devices[kMaxPorts];
};
