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

#include "components/Dialog.h"
#include "components/Input.h"

#include <SDL_hints.h>
#include <SDL_keycode.h>

#define MODIFIERS (KMOD_MODE | KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI)

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

  // Window size
  kSetWindowSize1,
  kSetWindowSize2,
  kSetWindowSize3,
  kSetWindowSize4,
  kToggleFullscreen,

  // Emulation speed
  kPauseToggle,
  kPauseToggleNoOvl,
  kFastForward,
  kFastForwardToggle,
  kStep,

  // Screenshot
  kScreenshot,

  kMaxBindings
};


bool KeyBinds::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  _slot = 1;
  _ff = false;

  if (SDL_NumJoysticks() > 0)
  {
    _bindings[kButtonUp] = { 0, SDL_CONTROLLER_BUTTON_DPAD_UP, Binding::Type::Button, 0 };
    _bindings[kButtonDown] = { 0, SDL_CONTROLLER_BUTTON_DPAD_DOWN, Binding::Type::Button, 0 };
    _bindings[kButtonLeft] = { 0, SDL_CONTROLLER_BUTTON_DPAD_LEFT, Binding::Type::Button, 0 };
    _bindings[kButtonRight] = { 0, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Binding::Type::Button, 0 };
    _bindings[kButtonX] = { 0, SDL_CONTROLLER_BUTTON_Y, Binding::Type::Button, 0 };
    _bindings[kButtonY] = { 0, SDL_CONTROLLER_BUTTON_X, Binding::Type::Button, 0 };
    _bindings[kButtonA] = { 0, SDL_CONTROLLER_BUTTON_B, Binding::Type::Button, 0 };
    _bindings[kButtonB] = { 0, SDL_CONTROLLER_BUTTON_A, Binding::Type::Button, 0 };
    _bindings[kButtonL] = { 0, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Binding::Type::Button, 0 };
    _bindings[kButtonR] = { 0, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Binding::Type::Button, 0 };
    _bindings[kButtonL2] = { 0, SDL_CONTROLLER_AXIS_TRIGGERLEFT, Binding::Type::Axis, 0 };
    _bindings[kButtonR2] = { 0, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, Binding::Type::Axis, 0 };
    _bindings[kButtonL3] = { 0, SDL_CONTROLLER_BUTTON_LEFTSTICK, Binding::Type::Button, 0 };
    _bindings[kButtonR3] = { 0, SDL_CONTROLLER_BUTTON_RIGHTSTICK, Binding::Type::Button, 0 };
    _bindings[kButtonSelect] = { 0, SDL_CONTROLLER_BUTTON_BACK, Binding::Type::Button, 0 };
    _bindings[kButtonStart] = { 0, SDL_CONTROLLER_BUTTON_START, Binding::Type::Button, 0 };
  }
  else
  {
    _bindings[kButtonUp] = { 0, SDLK_UP, Binding::Type::Key, 0 };
    _bindings[kButtonDown] = { 0, SDLK_DOWN, Binding::Type::Key, 0 };
    _bindings[kButtonLeft] = { 0, SDLK_LEFT, Binding::Type::Key, 0 };
    _bindings[kButtonRight] = { 0, SDLK_RIGHT, Binding::Type::Key, 0 };
    _bindings[kButtonX] = { 0, SDLK_s, Binding::Type::Key, 0 };
    _bindings[kButtonY] = { 0, SDLK_a, Binding::Type::Key, 0 };
    _bindings[kButtonA] = { 0, SDLK_x, Binding::Type::Key, 0 };
    _bindings[kButtonB] = { 0, SDLK_z, Binding::Type::Key, 0 };
    _bindings[kButtonL] = { 0, SDLK_d, Binding::Type::Key, 0 };
    _bindings[kButtonR] = { 0, SDLK_c, Binding::Type::Key, 0 };
    _bindings[kButtonL2] = { 0, SDLK_f, Binding::Type::Key, 0 };
    _bindings[kButtonR2] = { 0, SDLK_v, Binding::Type::Key, 0 };
    _bindings[kButtonL3] = { 0, SDLK_g, Binding::Type::Key, 0 };
    _bindings[kButtonR3] = { 0, SDLK_h, Binding::Type::Key, 0 };
    _bindings[kButtonSelect] = { 0, SDLK_TAB, Binding::Type::Key, 0 };
    _bindings[kButtonStart] = { 0, SDLK_RETURN, Binding::Type::Key, 0 };
  }

  _bindings[kSaveState1] = { 0, SDLK_F1, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState2] = { 0, SDLK_F2, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState3] = { 0, SDLK_F3, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState4] = { 0, SDLK_F4, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState5] = { 0, SDLK_F5, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState6] = { 0, SDLK_F6, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState7] = { 0, SDLK_F7, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState8] = { 0, SDLK_F8, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState9] = { 0, SDLK_F9, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSaveState10] = { 0, SDLK_F10, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kLoadState1] = { 0, SDLK_F1, Binding::Type::Key, 0 };
  _bindings[kLoadState2] = { 0, SDLK_F2, Binding::Type::Key, 0 };
  _bindings[kLoadState3] = { 0, SDLK_F3, Binding::Type::Key, 0 };
  _bindings[kLoadState4] = { 0, SDLK_F4, Binding::Type::Key, 0 };
  _bindings[kLoadState5] = { 0, SDLK_F5, Binding::Type::Key, 0 };
  _bindings[kLoadState6] = { 0, SDLK_F6, Binding::Type::Key, 0 };
  _bindings[kLoadState7] = { 0, SDLK_F7, Binding::Type::Key, 0 };
  _bindings[kLoadState8] = { 0, SDLK_F8, Binding::Type::Key, 0 };
  _bindings[kLoadState9] = { 0, SDLK_F9, Binding::Type::Key, 0 };
  _bindings[kLoadState10] = { 0, SDLK_F10, Binding::Type::Key, 0 };
  _bindings[kPreviousSlot] = { 0, SDLK_MINUS, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kNextSlot] = { 0, SDLK_EQUALS, Binding::Type::Key, KMOD_SHIFT };
  _bindings[kSetSlot1] = { 0, SDLK_1, Binding::Type::Key, 0 };
  _bindings[kSetSlot2] = { 0, SDLK_2, Binding::Type::Key, 0 };
  _bindings[kSetSlot3] = { 0, SDLK_3, Binding::Type::Key, 0 };
  _bindings[kSetSlot4] = { 0, SDLK_4, Binding::Type::Key, 0 };
  _bindings[kSetSlot5] = { 0, SDLK_5, Binding::Type::Key, 0 };
  _bindings[kSetSlot6] = { 0, SDLK_6, Binding::Type::Key, 0 };
  _bindings[kSetSlot7] = { 0, SDLK_7, Binding::Type::Key, 0 };
  _bindings[kSetSlot8] = { 0, SDLK_8, Binding::Type::Key, 0 };
  _bindings[kSetSlot9] = { 0, SDLK_9, Binding::Type::Key, 0 };
  _bindings[kSetSlot10] = { 0, SDLK_0, Binding::Type::Key, 0 };
  _bindings[kLoadCurrent] = { 0, SDLK_F11, Binding::Type::Key, 0 };
  _bindings[kSaveCurrent] = { 0, SDLK_F12, Binding::Type::Key, 0 };

  _bindings[kSetWindowSize1] = { 0, SDLK_1, Binding::Type::Key, KMOD_ALT };
  _bindings[kSetWindowSize2] = { 0, SDLK_2, Binding::Type::Key, KMOD_ALT };
  _bindings[kSetWindowSize3] = { 0, SDLK_3, Binding::Type::Key, KMOD_ALT };
  _bindings[kSetWindowSize4] = { 0, SDLK_4, Binding::Type::Key, KMOD_ALT };
  _bindings[kToggleFullscreen] = { 0, SDLK_RETURN, Binding::Type::Key, KMOD_ALT };

  _bindings[kPauseToggle] = { 0, SDLK_ESCAPE, Binding::Type::Key, 0 };
  _bindings[kPauseToggleNoOvl] = { 0, SDLK_p, Binding::Type::Key, 0 };
  _bindings[kFastForward] = { 0, SDLK_EQUALS, Binding::Type::Key, 0 };
  _bindings[kFastForwardToggle] = { 0, SDLK_MINUS, Binding::Type::Key, 0 };
  _bindings[kStep] = { 0, SDLK_SEMICOLON, Binding::Type::Key, 0 };

  _bindings[kScreenshot] = { 0, SDLK_PRINTSCREEN, Binding::Type::Key, 0 };

   // TODO: load persisted

  return true;
}

KeyBinds::Action KeyBinds::translateButtonPress(int button, unsigned* extra)
{
  switch (button)
  {
    // Joypad buttons
    case kButtonUp:     *extra = 1; return Action::kButtonUp;
    case kButtonDown:   *extra = 1; return Action::kButtonDown;
    case kButtonLeft:   *extra = 1; return Action::kButtonLeft;
    case kButtonRight:  *extra = 1; return Action::kButtonRight;
    case kButtonX:      *extra = 1; return Action::kButtonX;
    case kButtonY:      *extra = 1; return Action::kButtonY;
    case kButtonA:      *extra = 1; return Action::kButtonA;
    case kButtonB:      *extra = 1; return Action::kButtonB;
    case kButtonL:      *extra = 1; return Action::kButtonL;
    case kButtonR:      *extra = 1; return Action::kButtonR;
    case kButtonL2:     *extra = 1; return Action::kButtonL2;
    case kButtonR2:     *extra = 1; return Action::kButtonR2;
    case kButtonL3:     *extra = 1; return Action::kButtonL3;
    case kButtonR3:     *extra = 1; return Action::kButtonR3;
    case kButtonSelect: *extra = 1; return Action::kButtonSelect;
    case kButtonStart:  *extra = 1; return Action::kButtonStart;

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
    case kPreviousSlot: _slot = (_slot == 1) ? 10 : _slot - 1; return Action::kNothing;
    case kNextSlot:     _slot = (_slot == 10) ? 1 : _slot + 1; return Action::kNothing;
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

    // Window size
    case kSetWindowSize1:    return Action::kSetWindowSize1;
    case kSetWindowSize2:    return Action::kSetWindowSize2;
    case kSetWindowSize3:    return Action::kSetWindowSize3;
    case kSetWindowSize4:    return Action::kSetWindowSize4;
    case kToggleFullscreen:  return Action::kToggleFullscreen;

    // Emulation speed
    case kPauseToggleNoOvl:  return Action::kPauseToggleNoOvl;
    case kPauseToggle:       return Action::kPauseToggle;
    case kFastForward:       *extra = (unsigned)!_ff; return Action::kFastForward;
    case kFastForwardToggle: _ff = !_ff; *extra = (unsigned)_ff; return Action::kFastForward;
    case kStep:              return Action::kStep;

    // Screenshot
    case kScreenshot:        return Action::kScreenshot;

    default:                 return Action::kNothing;
  }
}

KeyBinds::Action KeyBinds::translateButtonReleased(int button, unsigned* extra)
{
  switch (button)
  {
    // Joypad buttons
    case kButtonUp:     *extra = 0; return Action::kButtonUp;
    case kButtonDown:   *extra = 0; return Action::kButtonDown;
    case kButtonLeft:   *extra = 0; return Action::kButtonLeft;
    case kButtonRight:  *extra = 0; return Action::kButtonRight;
    case kButtonX:      *extra = 0; return Action::kButtonX;
    case kButtonY:      *extra = 0; return Action::kButtonY;
    case kButtonA:      *extra = 0; return Action::kButtonA;
    case kButtonB:      *extra = 0; return Action::kButtonB;
    case kButtonL:      *extra = 0; return Action::kButtonL;
    case kButtonR:      *extra = 0; return Action::kButtonR;
    case kButtonL2:     *extra = 0; return Action::kButtonL2;
    case kButtonR2:     *extra = 0; return Action::kButtonR2;
    case kButtonL3:     *extra = 0; return Action::kButtonL3;
    case kButtonR3:     *extra = 0; return Action::kButtonR3;
    case kButtonSelect: *extra = 0; return Action::kButtonSelect;
    case kButtonStart:  *extra = 0; return Action::kButtonStart;

    // Emulation speed
    case kFastForward: *extra = (unsigned)_ff; return Action::kFastForward;

    default: return Action::kNothing;
  }
}

KeyBinds::Action KeyBinds::translate(const SDL_KeyboardEvent* key, unsigned* extra)
{
  if (key->repeat)
  {
    return Action::kNothing;
  }

  // Accept both left and right modifiers
  Uint16 mod = key->keysym.mod & MODIFIERS;
  if (mod & KMOD_SHIFT) mod |= KMOD_SHIFT;
  if (mod & KMOD_CTRL)  mod |= KMOD_CTRL;
  if (mod & KMOD_ALT)   mod |= KMOD_ALT;
  if (mod & KMOD_GUI)   mod |= KMOD_GUI;

  for (size_t i = 0; i < kMaxBindings; i++)
  {
    if (_bindings[i].type == Binding::Type::Key)
    {
      if (key->keysym.sym == _bindings[i].button && mod == _bindings[i].modifiers)
      {
        if (key->state == SDL_PRESSED)
          return translateButtonPress(i, extra);

        if (key->state == SDL_RELEASED)
          return translateButtonReleased(i, extra);

        break;
      }
    }
  }

  return Action::kNothing;
}

KeyBinds::Action KeyBinds::translate(const SDL_ControllerButtonEvent* cbutton, unsigned* extra)
{
  for (size_t i = 0; i < kMaxBindings; i++)
  {
    if (_bindings[i].type == Binding::Type::Button)
    {
      if (cbutton->button == _bindings[i].button && cbutton->which == _bindings[i].joystick_id)
      {
        if (cbutton->state == SDL_PRESSED)
          return translateButtonPress(i, extra);

        if (cbutton->state == SDL_RELEASED)
          return translateButtonReleased(i, extra);

        break;
      }
    }
  }

  return Action::kNothing;
}

void KeyBinds::translate(const SDL_ControllerAxisEvent* caxis, Input& input,
  Action* action1, unsigned* extra1, Action* action2, unsigned* extra2)
{
  *action1 = *action2 = Action::kNothing;

  int threshold = 32767 * input.getJoystickSensitivity(caxis->which);
  for (size_t i = 0; i < kMaxBindings; i++)
  {
    if (_bindings[i].type == Binding::Type::Axis)
    {
      if (caxis->axis == _bindings[i].button && caxis->which == _bindings[i].joystick_id)
      {
        if ((_bindings[i].modifiers & 0xFF) == 0xFF) // negative axis
        {
          if (caxis->value < -threshold)
            *action1 = translateButtonPress(i, extra1);
          else
            *action1 = translateButtonReleased(i, extra1);
        }
        else // positive axis
        {
          if (caxis->value > threshold)
            *action2 = translateButtonPress(i, extra2);
          else
            *action2 = translateButtonReleased(i, extra2);
        }
      }
    }
  }
}

// SDL defines a function to convert a virtual key to a SDL scancode, but it's not public.
// * SDL_Scancode WindowsScanCodeToSDLScanCode(LPARAM lParam, WPARAM wParam);
//
// we don't actually care about the extended keys, so we can get by just duplicating the raw keys
// from http://hg.libsdl.org/SDL/file/131ea7dcc225/src/events/scancodes_windows.h
static const SDL_Scancode windows_scancode_table[] =
{
  /*	0						1							2							3							4						5							6							7 */
  /*	8						9							A							B							C						D							E							F */
  SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_ESCAPE,		SDL_SCANCODE_1,				SDL_SCANCODE_2,				SDL_SCANCODE_3,			SDL_SCANCODE_4,				SDL_SCANCODE_5,				SDL_SCANCODE_6,			/* 0 */
  SDL_SCANCODE_7,				SDL_SCANCODE_8,				SDL_SCANCODE_9,				SDL_SCANCODE_0,				SDL_SCANCODE_MINUS,		SDL_SCANCODE_EQUALS,		SDL_SCANCODE_BACKSPACE,		SDL_SCANCODE_TAB,		/* 0 */

  SDL_SCANCODE_Q,				SDL_SCANCODE_W,				SDL_SCANCODE_E,				SDL_SCANCODE_R,				SDL_SCANCODE_T,			SDL_SCANCODE_Y,				SDL_SCANCODE_U,				SDL_SCANCODE_I,			/* 1 */
  SDL_SCANCODE_O,				SDL_SCANCODE_P,				SDL_SCANCODE_LEFTBRACKET,	SDL_SCANCODE_RIGHTBRACKET,	SDL_SCANCODE_RETURN,	SDL_SCANCODE_LCTRL,			SDL_SCANCODE_A,				SDL_SCANCODE_S,			/* 1 */

  SDL_SCANCODE_D,				SDL_SCANCODE_F,				SDL_SCANCODE_G,				SDL_SCANCODE_H,				SDL_SCANCODE_J,			SDL_SCANCODE_K,				SDL_SCANCODE_L,				SDL_SCANCODE_SEMICOLON,	/* 2 */
  SDL_SCANCODE_APOSTROPHE,	SDL_SCANCODE_GRAVE,			SDL_SCANCODE_LSHIFT,		SDL_SCANCODE_BACKSLASH,		SDL_SCANCODE_Z,			SDL_SCANCODE_X,				SDL_SCANCODE_C,				SDL_SCANCODE_V,			/* 2 */

  SDL_SCANCODE_B,				SDL_SCANCODE_N,				SDL_SCANCODE_M,				SDL_SCANCODE_COMMA,			SDL_SCANCODE_PERIOD,	SDL_SCANCODE_SLASH,			SDL_SCANCODE_RSHIFT,		SDL_SCANCODE_PRINTSCREEN,/* 3 */
  SDL_SCANCODE_LALT,			SDL_SCANCODE_SPACE,			SDL_SCANCODE_CAPSLOCK,		SDL_SCANCODE_F1,			SDL_SCANCODE_F2,		SDL_SCANCODE_F3,			SDL_SCANCODE_F4,			SDL_SCANCODE_F5,		/* 3 */

  SDL_SCANCODE_F6,			SDL_SCANCODE_F7,			SDL_SCANCODE_F8,			SDL_SCANCODE_F9,			SDL_SCANCODE_F10,		SDL_SCANCODE_NUMLOCKCLEAR,	SDL_SCANCODE_SCROLLLOCK,	SDL_SCANCODE_HOME,		/* 4 */
  SDL_SCANCODE_UP,			SDL_SCANCODE_PAGEUP,		SDL_SCANCODE_KP_MINUS,		SDL_SCANCODE_LEFT,			SDL_SCANCODE_KP_5,		SDL_SCANCODE_RIGHT,			SDL_SCANCODE_KP_PLUS,		SDL_SCANCODE_END,		/* 4 */

  SDL_SCANCODE_DOWN,			SDL_SCANCODE_PAGEDOWN,		SDL_SCANCODE_INSERT,		SDL_SCANCODE_DELETE,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_NONUSBACKSLASH,SDL_SCANCODE_F11,		/* 5 */
  SDL_SCANCODE_F12,			SDL_SCANCODE_PAUSE,			SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_LGUI,			SDL_SCANCODE_RGUI,		SDL_SCANCODE_APPLICATION,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 5 */

  SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_F13,		SDL_SCANCODE_F14,			SDL_SCANCODE_F15,			SDL_SCANCODE_F16,		/* 6 */
  SDL_SCANCODE_F17,			SDL_SCANCODE_F18,			SDL_SCANCODE_F19,			SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 6 */

  SDL_SCANCODE_INTERNATIONAL2,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL1,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 7 */
  SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL4,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL5,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_INTERNATIONAL3,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN	/* 7 */
};

// subset of method here: http://hg.libsdl.org/SDL/file/131ea7dcc225/src/video/windows/SDL_windowsevents.c
static SDL_Scancode WindowsScanCodeToSDLScanCode(LPARAM lparam, WPARAM wparam)
{
  switch (wparam)
  {
    case VK_PAUSE: return SDL_SCANCODE_PAUSE;
    case VK_NUMLOCK: return SDL_SCANCODE_NUMLOCKCLEAR;
  }

  int nScanCode = (lparam >> 16) & 0xFF;
  SDL_Scancode code = (nScanCode <= 127) ? windows_scancode_table[nScanCode] : SDL_SCANCODE_UNKNOWN;

  if (lparam & (1 << 24))
  {
    switch (code) {
      case SDL_SCANCODE_RETURN:
        code = SDL_SCANCODE_KP_ENTER;
        break;
      case SDL_SCANCODE_LALT:
        code = SDL_SCANCODE_RALT;
        break;
      case SDL_SCANCODE_LCTRL:
        code = SDL_SCANCODE_RCTRL;
        break;
      case SDL_SCANCODE_SLASH:
        code = SDL_SCANCODE_KP_DIVIDE;
        break;
      case SDL_SCANCODE_CAPSLOCK:
        code = SDL_SCANCODE_KP_PLUS;
        break;
      default:
        break;
    }
  }
  else
  {
    switch (code) {
      case SDL_SCANCODE_HOME:
        code = SDL_SCANCODE_KP_7;
        break;
      case SDL_SCANCODE_UP:
        code = SDL_SCANCODE_KP_8;
        break;
      case SDL_SCANCODE_PAGEUP:
        code = SDL_SCANCODE_KP_9;
        break;
      case SDL_SCANCODE_LEFT:
        code = SDL_SCANCODE_KP_4;
        break;
      case SDL_SCANCODE_RIGHT:
        code = SDL_SCANCODE_KP_6;
        break;
      case SDL_SCANCODE_END:
        code = SDL_SCANCODE_KP_1;
        break;
      case SDL_SCANCODE_DOWN:
        code = SDL_SCANCODE_KP_2;
        break;
      case SDL_SCANCODE_PAGEDOWN:
        code = SDL_SCANCODE_KP_3;
        break;
      case SDL_SCANCODE_INSERT:
        code = SDL_SCANCODE_KP_0;
        break;
      case SDL_SCANCODE_DELETE:
        code = SDL_SCANCODE_KP_PERIOD;
        break;
      case SDL_SCANCODE_PRINTSCREEN:
        code = SDL_SCANCODE_KP_MULTIPLY;
        break;
      default:
        break;
    }
  }

  return code;
}

class ChangeInputDialog : public Dialog
{
public:
  static const WORD ID_LABEL = 40000;
  static const WORD IDC_CLEAR = 50000;
  static const WORD IDT_TIMER = 40500;

  const KeyBinds::Binding getButtonDescriptor() const { return _buttonDescriptor; }

  static void getButtonLabel(char buffer[32], KeyBinds::Binding& desc)
  {
    switch (desc.type)
    {
      case KeyBinds::Binding::Type::None:
        sprintf(buffer, "none");
        break;

      case KeyBinds::Binding::Type::Button:
        sprintf(buffer, "J%d %s", desc.joystick_id, SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(desc.button)));
        break;

      case KeyBinds::Binding::Type::Axis:
        switch (desc.modifiers)
        {
          case 0xFF:
            sprintf(buffer, "J%d -%s", desc.joystick_id, SDL_GameControllerGetStringForAxis(static_cast<SDL_GameControllerAxis>(desc.button)));
            break;
          case 0:
            sprintf(buffer, "J%d %s", desc.joystick_id, SDL_GameControllerGetStringForAxis(static_cast<SDL_GameControllerAxis>(desc.button)));
            break;
          case 1:
            sprintf(buffer, "J%d +%s", desc.joystick_id, SDL_GameControllerGetStringForAxis(static_cast<SDL_GameControllerAxis>(desc.button)));
            break;
        }
        break;

      case KeyBinds::Binding::Type::Key:
        sprintf(buffer, "%s%s%s%s",
          (desc.modifiers & KMOD_CTRL) ? "Ctrl+" : "",
          (desc.modifiers & KMOD_ALT) ? "Alt+" : "",
          (desc.modifiers & KMOD_SHIFT) ? "Shift+" : "",
          SDL_GetKeyName(static_cast<SDL_Keycode>(desc.button)));
        break;
    }
  }

  Input* _input = nullptr;
  bool _isOpen;

  bool show(HWND hParent)
  {
    auto hwnd = CreateDialogIndirectParam(NULL, (LPCDLGTEMPLATE)_template, hParent, s_dialogProc, (LPARAM)this);
    EnableWindow(hParent, 0);

    ShowWindow(hwnd, 1);

    _isOpen = true;
    while(_isOpen)
    {
      MSG msg;
      while(PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
      {
        if(GetMessage(&msg, 0, 0, 0) > 0)
        {
          switch (msg.message)
          {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
              const auto code = WindowsScanCodeToSDLScanCode(msg.lParam, msg.wParam);
              const auto sdlKey = SDL_GetKeyFromScancode(code);
              switch (sdlKey)
              {
                case SDLK_UNKNOWN:
                case SDLK_LALT:
                case SDLK_RALT:
                case SDLK_LCTRL:
                case SDLK_RCTRL:
                case SDLK_LSHIFT:
                case SDLK_RSHIFT:
                  break;

                default:
                  _buttonDescriptor.type = KeyBinds::Binding::Key;
                  _buttonDescriptor.joystick_id = -1;
                  _buttonDescriptor.button = sdlKey;
                  _buttonDescriptor.modifiers = 0;

                  if (GetKeyState(VK_CONTROL) & 0x8000)
                    _buttonDescriptor.modifiers |= KMOD_CTRL;
                  if (GetKeyState(VK_MENU) & 0x8000)
                    _buttonDescriptor.modifiers |= KMOD_ALT;
                  if (GetKeyState(VK_SHIFT) & 0x8000)
                    _buttonDescriptor.modifiers |= KMOD_SHIFT;

                  char buffer[32];
                  getButtonLabel(buffer, _buttonDescriptor);
                  SetDlgItemText(hwnd, ID_LABEL, buffer);
                  break;
              }
            }

            case WM_SYSCOMMAND:
              break;

            default:
              if(!IsDialogMessage(hwnd, &msg))
              {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
              }
              break;
          }
        }
      }
      Sleep(10);
    }

    EnableWindow(hParent, 1);

    return true;
  }

protected:
  KeyBinds::Binding _buttonDescriptor;

  void initControls(HWND hwnd) override
  {
    Dialog::initControls(hwnd);

    SetTimer(hwnd, IDT_TIMER, 100, (TIMERPROC)NULL);
  }

  void retrieveData(HWND hwnd) override
  {
    Dialog::retrieveData(hwnd);

    // _updated is normally only set if a field changes, since this dialog has no fields,
    // always return "modified" if the user didn't cancel the dialog.
    _updated = true;
  }

  void markClosed(HWND hwnd) override
  {
    Dialog::markClosed(hwnd);
    _isOpen = false;
  }

  INT_PTR dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override
  {
    switch (msg)
    {
      case WM_COMMAND:
      {
        int controlId = LOWORD(wparam);
        if (controlId == IDC_CLEAR)
        {
          _buttonDescriptor.joystick_id = 0;
          _buttonDescriptor.type = KeyBinds::Binding::Type::None;
          _buttonDescriptor.button = 0;
          _buttonDescriptor.modifiers = 0;

          char buffer[32];
          getButtonLabel(buffer, _buttonDescriptor);
          SetDlgItemText(hwnd, ID_LABEL, buffer);
        }
        break;
      }

      case WM_TIMER:
        if (wparam == IDT_TIMER)
        {
          KeyBinds::Binding button = _input->captureButtonPress();
          if (button.type != KeyBinds::Binding::Type::None)
          {
            _buttonDescriptor = button;
            char buffer[32];
            getButtonLabel(buffer, _buttonDescriptor);
            SetDlgItemText(hwnd, ID_LABEL, buffer);
          }
        }
        break;
    }

    return Dialog::dialogProc(hwnd, msg, wparam, lparam);
  }
};

class InputDialog : public Dialog
{
public:
  void initControllerButtons(const KeyBinds::BindingList& bindings)
  {
    _bindings = bindings;

    const WORD WIDTH = 478;
    const WORD HEIGHT = 144;

    addButtonInput(0, 1, "L2", kButtonL2);
    addButtonInput(0, 9, "R2", kButtonR2);
    addButtonInput(1, 1, "L1", kButtonL);
    addButtonInput(1, 9, "R1", kButtonR);
    addButtonInput(2, 1, "Up", kButtonUp);
    addButtonInput(2, 4, "Select", kButtonSelect);
    addButtonInput(2, 6, "Start", kButtonStart);
    addButtonInput(2, 9, "X", kButtonX);
    addButtonInput(3, 0, "Left", kButtonLeft);
    addButtonInput(3, 2, "Right", kButtonRight);
    addButtonInput(3, 8, "Y", kButtonY);
    addButtonInput(3, 10, "A", kButtonA);
    addButtonInput(4, 1, "Down", kButtonDown);
    addButtonInput(4, 9, "B", kButtonB);

    addButton("OK", IDOK, WIDTH - 55 - 50, HEIGHT - 14, 50, 14, true);
    addButton("Cancel", IDCANCEL, WIDTH - 50, HEIGHT - 14, 50, 14, false);
  }

  void initHotKeyButtons(const KeyBinds::BindingList& bindings)
  {
    _bindings = bindings;

    const WORD WIDTH = 357;
    const WORD HEIGHT = 325;
    char label[32];

    addButtonInput(0, 0, "Window Size 1x", kSetWindowSize1);
    addButtonInput(1, 0, "Window Size 2x", kSetWindowSize2);
    addButtonInput(2, 0, "Window Size 3x", kSetWindowSize3);
    addButtonInput(3, 0, "Window Size 4x", kSetWindowSize4);
    addButtonInput(4, 0, "Toggle Fullscreen", kToggleFullscreen);

    addButtonInput(5, 0, "Show Overlay", kPauseToggle);
    addButtonInput(6, 0, "Pause", kPauseToggleNoOvl);
    addButtonInput(7, 0, "Frame Advance", kStep);
    addButtonInput(8, 0, "Fast Forward (Hold)", kFastForward);
    addButtonInput(9, 0, "Fast Forward (Toggle)", kFastForwardToggle);

    addButtonInput(10, 0, "Take Screenshot", kScreenshot);

    for (int i = 0; i < 10; ++i)
    {
      sprintf(label, "Save State %d", i + 1);
      addButtonInput(i, 3, label, kSaveState1 + i);
    }
    addButtonInput(10, 3, "Save Current State", kSaveCurrent);

    for (int i = 0; i < 10; ++i)
    {
      sprintf(label, "Load State %d", i + 1);
      addButtonInput(i, 5, label, kLoadState1 + i);
    }
    addButtonInput(10, 5, "Load Current State", kLoadCurrent);

    for (int i = 0; i < 10; ++i)
    {
      sprintf(label, "Select State %d", i + 1);
      addButtonInput(i, 7, label, kSetSlot1 + i);
    }
    addButtonInput(10, 7, "Select Previous State", kPreviousSlot);
    addButtonInput(11, 7, "Select Next State", kNextSlot);

    addButton("OK", IDOK, WIDTH - 55 - 50, HEIGHT - 14, 50, 14, true);
    addButton("Cancel", IDCANCEL, WIDTH - 50, HEIGHT - 14, 50, 14, false);
  }

  Input* _input = nullptr;

  const KeyBinds::BindingList& getBindings() const { return _bindings; }

protected:
  KeyBinds::BindingList _bindings = {};
  std::array<char[32], kMaxBindings> _buttonLabels = {};

  void retrieveData(HWND hwnd) override
  {
    Dialog::retrieveData(hwnd);

    // _updated is normally only set if a field changes, since this dialog has no fields,
    // always return "modified" if the user didn't cancel the dialog.
    _updated = true;
  }

  void updateButtonLabel(int button)
  {
    KeyBinds::Binding& desc = _bindings[button];
    ChangeInputDialog::getButtonLabel(_buttonLabels[button], desc);
  }

  void addButtonInput(int row, int column, const char* label, int button)
  {
    const WORD ITEM_WIDTH = 80;
    const WORD ITEM_HEIGHT = 10;

    const WORD x = column * (ITEM_WIDTH / 2) + 3;
    const WORD y = row * (ITEM_HEIGHT * 2 + 6);

    addButton(label, 20000 + button, x, y, ITEM_WIDTH -6 , ITEM_HEIGHT, false);

    updateButtonLabel(button);
    addEditbox(10000 + button, x, y + ITEM_HEIGHT, ITEM_WIDTH - 6, ITEM_HEIGHT, 1, _buttonLabels[button], 16, true);
  }

  INT_PTR dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override
  {
    switch (msg)
    {
      case WM_COMMAND:
      {
        int controlId = LOWORD(wparam);
        if (controlId >= 20000 && controlId <= 20000 + kMaxBindings)
          updateButton(hwnd, controlId - 20000);
        break;
      }
    }

    return Dialog::dialogProc(hwnd, msg, wparam, lparam);
  }

  void updateButton(HWND hwnd, int button)
  {
    char buffer[32];
    GetDlgItemText(hwnd, 20000 + button, buffer, sizeof(buffer));

    ChangeInputDialog db;
    db.init(buffer);
    db._input = _input;

    GetDlgItemText(hwnd, 10000 + button, buffer, sizeof(buffer));
    db.addLabel(buffer, ChangeInputDialog::ID_LABEL, 0, 0, 100, 15);

    db.addButton("Clear", ChangeInputDialog::IDC_CLEAR, 0, 20, 50, 14, false);
    db.addButton("OK", IDOK, 80, 20, 50, 14, true);
    db.addButton("Cancel", IDCANCEL, 140, 20, 50, 14, false);

    // the joystick is normally ignored if the SDL window doesn't have focus
    // temporarily disable that behavior
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (db.show(hwnd))
    {
      _bindings[button] = db.getButtonDescriptor();
      updateButtonLabel(button);
      SetDlgItemText(hwnd, 10000 + button, _buttonLabels[button]);
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
  }
};

void KeyBinds::showControllerDialog(Input& input, int port)
{
  char label[32];
  snprintf(label, sizeof(label), "Controller %u", port + 1);

  InputDialog db;
  db.init(label);
  db._input = &input;
  db.initControllerButtons(_bindings);

  if (db.show())
  {
    _bindings = db.getBindings();
    // TODO: persist
  }
}

void KeyBinds::showHotKeyDialog(Input& input)
{
  InputDialog db;
  db.init("Hot Keys");
  db._input = &input;
  db.initHotKeyButtons(_bindings);

  if (db.show())
  {
    _bindings = db.getBindings();
    // TODO: persist
  }
}
