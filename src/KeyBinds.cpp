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

#include "KeyBinds.h"

#include <SDL_keycode.h>

#define MODIFIERS (KMOD_MODE | KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI)

bool KeyBinds::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  _slot = 1;
  _ff = false;

  return true;
}

KeyBinds::Action KeyBinds::translate(const SDL_KeyboardEvent* key, unsigned* extra)
{
  enum
  {
    // Joypad buttons
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
    // State management
    kSaveState1,
    kSaveState2,
    kSaveState3,
    kSaveState4,
    kSaveState5,
    kSaveState6,
    kSaveState7,
    kSaveState8,
    kSaveState9,
    kSaveState10,
    kLoadState1,
    kLoadState2,
    kLoadState3,
    kLoadState4,
    kLoadState5,
    kLoadState6,
    kLoadState7,
    kLoadState8,
    kLoadState9,
    kLoadState10,
    kPreviousSlot,
    kNextSlot,
    kSetSlot1,
    kSetSlot2,
    kSetSlot3,
    kSetSlot4,
    kSetSlot5,
    kSetSlot6,
    kSetSlot7,
    kSetSlot8,
    kSetSlot9,
    kSetSlot10,
    kLoadCurrent,
    kSaveCurrent,
    // Emulation speed
    kPauseToggle,
    kFastForward,
    kFastForwardToggle,
    kStep,
    // Screenshot
    kScreenshot
  };

  static const struct { SDL_Keycode sym; Uint16 mod; int action; } bindings[] =
  {
    // Joypad buttons
    {SDLK_UP,          0,          kButtonUp},
    {SDLK_DOWN,        0,          kButtonDown},
    {SDLK_LEFT,        0,          kButtonLeft},
    {SDLK_RIGHT,       0,          kButtonRight},
    {SDLK_s,           0,          kButtonX},
    {SDLK_a,           0,          kButtonY},
    {SDLK_x,           0,          kButtonA},
    {SDLK_z,           0,          kButtonB},
    {SDLK_d,           0,          kButtonL},
    {SDLK_c,           0,          kButtonR},
    {SDLK_f,           0,          kButtonL2},
    {SDLK_v,           0,          kButtonR2},
    {SDLK_g,           0,          kButtonL3},
    {SDLK_h,           0,          kButtonR3},
    {SDLK_TAB,         0,          kButtonSelect},
    {SDLK_RETURN,      0,          kButtonStart},
    // State management
    {SDLK_F1,          0,          kLoadState1},
    {SDLK_F2,          0,          kLoadState2},
    {SDLK_F3,          0,          kLoadState3},
    {SDLK_F4,          0,          kLoadState4},
    {SDLK_F5,          0,          kLoadState5},
    {SDLK_F6,          0,          kLoadState6},
    {SDLK_F7,          0,          kLoadState7},
    {SDLK_F8,          0,          kLoadState8},
    {SDLK_F9,          0,          kLoadState9},
    {SDLK_F10,         0,          kLoadState10},
    {SDLK_F1,          KMOD_SHIFT, kSaveState1},
    {SDLK_F2,          KMOD_SHIFT, kSaveState2},
    {SDLK_F3,          KMOD_SHIFT, kSaveState3},
    {SDLK_F4,          KMOD_SHIFT, kSaveState4},
    {SDLK_F5,          KMOD_SHIFT, kSaveState5},
    {SDLK_F6,          KMOD_SHIFT, kSaveState6},
    {SDLK_F7,          KMOD_SHIFT, kSaveState7},
    {SDLK_F8,          KMOD_SHIFT, kSaveState8},
    {SDLK_F9,          KMOD_SHIFT, kSaveState9},
    {SDLK_F10,         KMOD_SHIFT, kSaveState10},
    {SDLK_MINUS,       KMOD_SHIFT, kPreviousSlot},
    {SDLK_EQUALS,      KMOD_SHIFT, kNextSlot},
    {SDLK_1,           0,          kSetSlot1},
    {SDLK_2,           0,          kSetSlot2},
    {SDLK_3,           0,          kSetSlot3},
    {SDLK_4,           0,          kSetSlot4},
    {SDLK_5,           0,          kSetSlot5},
    {SDLK_6,           0,          kSetSlot6},
    {SDLK_7,           0,          kSetSlot7},
    {SDLK_8,           0,          kSetSlot8},
    {SDLK_9,           0,          kSetSlot9},
    {SDLK_0,           0,          kSetSlot10},
    {SDLK_F11,         0,          kLoadCurrent},
    {SDLK_BACKSPACE,   0,          kSaveCurrent},
    // Emulation speed
    {SDLK_p,           0,          kPauseToggle},
    {SDLK_EQUALS,      0,          kFastForward},
    {SDLK_MINUS,       0,          kFastForwardToggle},
    {SDLK_SEMICOLON,   0,          kStep},
    // Screenshot
    {SDLK_PRINTSCREEN, 0,          kScreenshot}
  };

  if (key->repeat)
  {
    return Action::kNothing;
  }

  for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++)
  {
    Uint16 mod = key->keysym.mod & MODIFIERS;

    // Accept both left and right modifiers
    if (mod & KMOD_SHIFT) mod |= KMOD_SHIFT;
    if (mod & KMOD_CTRL)  mod |= KMOD_CTRL;
    if (mod & KMOD_ALT)   mod |= KMOD_ALT;
    if (mod & KMOD_GUI)   mod |= KMOD_GUI;

    if (key->keysym.sym == bindings[i].sym && mod == bindings[i].mod)
    {
      switch (bindings[i].action)
      {
      // Joypad buttons
      case kButtonUp:     *extra = key->state == SDL_PRESSED; return Action::kButtonUp;
      case kButtonDown:   *extra = key->state == SDL_PRESSED; return Action::kButtonDown;
      case kButtonLeft:   *extra = key->state == SDL_PRESSED; return Action::kButtonLeft;
      case kButtonRight:  *extra = key->state == SDL_PRESSED; return Action::kButtonRight;
      case kButtonX:      *extra = key->state == SDL_PRESSED; return Action::kButtonX;
      case kButtonY:      *extra = key->state == SDL_PRESSED; return Action::kButtonY;
      case kButtonA:      *extra = key->state == SDL_PRESSED; return Action::kButtonA;
      case kButtonB:      *extra = key->state == SDL_PRESSED; return Action::kButtonB;
      case kButtonL:      *extra = key->state == SDL_PRESSED; return Action::kButtonL;
      case kButtonR:      *extra = key->state == SDL_PRESSED; return Action::kButtonR;
      case kButtonL2:     *extra = key->state == SDL_PRESSED; return Action::kButtonL2;
      case kButtonR2:     *extra = key->state == SDL_PRESSED; return Action::kButtonR2;
      case kButtonL3:     *extra = key->state == SDL_PRESSED; return Action::kButtonL3;
      case kButtonR3:     *extra = key->state == SDL_PRESSED; return Action::kButtonR3;
      case kButtonSelect: *extra = key->state == SDL_PRESSED; return Action::kButtonSelect;
      case kButtonStart:  *extra = key->state == SDL_PRESSED; return Action::kButtonStart;
      }

      if (key->state == SDL_PRESSED)
      {
        switch (bindings[i].action)
        {
        // State state management
        case kSaveState1:   *extra = 1; return Action::kSaveState;
        case kSaveState2:   *extra = 2; return Action::kSaveState;
        case kSaveState3:   *extra = 3; return Action::kSaveState;
        case kSaveState4:   *extra = 4; return Action::kSaveState;
        case kSaveState5:   *extra = 5; return Action::kSaveState;
        case kSaveState6:   *extra = 6; return Action::kSaveState;
        case kSaveState7:   *extra = 7; return Action::kSaveState;
        case kSaveState8:   *extra = 8; return Action::kSaveState;
        case kSaveState9:   *extra = 9; return Action::kSaveState;
        case kSaveState10:  *extra = 10; return Action::kSaveState;
        case kLoadState1:   *extra = 1; return Action::kLoadState;
        case kLoadState2:   *extra = 2; return Action::kLoadState;
        case kLoadState3:   *extra = 3; return Action::kLoadState;
        case kLoadState4:   *extra = 4; return Action::kLoadState;
        case kLoadState5:   *extra = 5; return Action::kLoadState;
        case kLoadState6:   *extra = 6; return Action::kLoadState;
        case kLoadState7:   *extra = 7; return Action::kLoadState;
        case kLoadState8:   *extra = 8; return Action::kLoadState;
        case kLoadState9:   *extra = 9; return Action::kLoadState;
        case kLoadState10:  *extra = 10; return Action::kLoadState;
        case kPreviousSlot: _slot = _slot == 1 ? 10 : _slot - 1; return Action::kNothing;
        case kNextSlot:     _slot = _slot == 10 ? 1 : _slot + 1; return Action::kNothing;
        case kSetSlot1:     _slot = 1; return Action::kNothing;
        case kSetSlot2:     _slot = 2; return Action::kNothing;
        case kSetSlot3:     _slot = 3; return Action::kNothing;
        case kSetSlot4:     _slot = 4; return Action::kNothing;
        case kSetSlot5:     _slot = 5; return Action::kNothing;
        case kSetSlot6:     _slot = 6; return Action::kNothing;
        case kSetSlot7:     _slot = 7; return Action::kNothing;
        case kSetSlot8:     _slot = 8; return Action::kNothing;
        case kSetSlot9:     _slot = 9; return Action::kNothing;
        case kSetSlot10:    _slot = 10; return Action::kNothing;
        case kLoadCurrent:  *extra = _slot; return Action::kLoadState;
        case kSaveCurrent:  *extra = _slot; return Action::kSaveState;
        // Emulation speed
        case kPauseToggle:       return Action::kPauseToggle;
        case kFastForward:       *extra = (unsigned)!_ff; return Action::kFastForward;
        case kFastForwardToggle: _ff = !_ff; *extra = (unsigned)_ff; return Action::kFastForward;
        case kStep:              return Action::kStep;
        // Screenshot
        case kScreenshot: return Action::kScreenshot;
        }
      }
      else if (key->state == SDL_RELEASED)
      {
        switch (bindings[i].action)
        {
        // Emulation speed
        case kFastForward: *extra = (unsigned)_ff; return Action::kFastForward;
        }
      }
    }
  }

  return Action::kNothing;
}
