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

#include <SDL_events.h>

#include <array>
#include <map>

class Input; // forward reference

class KeyBinds
{
public:
  enum class Action
  {
    kNothing,

    // Joypad buttons (extra = port << 8 | pressed)
    kButtonUp,
    kButtonDown,
    kButtonLeft,
    kButtonRight,
    kButtonX,
    kButtonY,
    kButtonA,
    kButtonB,
    kButtonL,
    kButtonR,
    kButtonL2,
    kButtonR2,
    kButtonL3,
    kButtonR3,
    kButtonSelect,
    kButtonStart,

    // Joypad analog sticks
    kAxisLeftX,
    kAxisLeftY,
    kAxisRightX,
    kAxisRightY,

    // State state management (extra = slot)
    kSaveState,
    kLoadState,

    // Window size
    kSetWindowSize1,
    kSetWindowSize2,
    kSetWindowSize3,
    kSetWindowSize4,
    kSetWindowSize5,
    kToggleFullscreen,
    kRotateRight,
    kRotateLeft,

    // Emulation speed
    kPauseToggle,
    kPauseToggleNoOvl,
    kFastForward, // (extra = pressed[0/1],toggle[2])
    kStep,

    // Screenshot
    kScreenshot,

    // Reset
    kReset,

    // Keyboard
    kGameFocusToggle,
    kKeyboardInput  // (extra = key << 8 | pressed)
  };

  bool init(libretro::LoggerComponent* logger);
  void destroy() {}

  Action translate(const SDL_KeyboardEvent* event, unsigned* extra);
  Action translate(const SDL_ControllerButtonEvent* event, unsigned* extra);
  void translate(const SDL_ControllerAxisEvent* caxis, Input& input,
    Action* action1, unsigned* extra1, Action* action2, unsigned* extra2);

  void mapDevice(SDL_JoystickID originalID, SDL_JoystickID newID);
  unsigned getNavigationPort(SDL_JoystickID joystickID);

  void showControllerDialog(Input& input, int portId);
  void showHotKeyDialog(Input& input);

  struct Binding
  {
    enum Type
    {
      None = 0,
      Button,
      Axis,
      Key,
    };

    SDL_JoystickID joystick_id;
    uint32_t button;
    Type type;
    uint16_t modifiers;
  };
  typedef std::array<Binding, 90> BindingList;

  static void getBindingString(char buffer[32], const KeyBinds::Binding& desc);

  std::string serializeBindings() const;
  bool deserializeBindings(const char* json);

  bool hasGameFocus() const noexcept { return _gameFocus; }

protected:
  libretro::LoggerComponent* _logger;

  KeyBinds::Action translateButtonPress(int button, unsigned* extra);
  KeyBinds::Action translateButtonReleased(int button, unsigned* extra);
  KeyBinds::Action translateAnalog(int button, Sint16 value, unsigned* extra);
  KeyBinds::Action translateKeyboardInput(SDL_Keycode kcode, bool pressed, unsigned* extra) const;

  BindingList _bindings;
  SDL_JoystickID getBindingID(SDL_JoystickID id) const;

  std::map<SDL_JoystickID, SDL_JoystickID> _bindingMap;

  unsigned _slot;
  bool _gameFocus;
};

