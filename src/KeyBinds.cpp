#include "KeyBinds.h"

#include <SDL_keycode.h>

#define BUTTON_UP     SDLK_UP
#define BUTTON_DOWN   SDLK_DOWN
#define BUTTON_LEFT   SDLK_LEFT
#define BUTTON_RIGHT  SDLK_RIGHT
#define BUTTON_X      SDLK_s
#define BUTTON_Y      SDLK_a
#define BUTTON_A      SDLK_x
#define BUTTON_B      SDLK_z
#define BUTTON_L      SDLK_d
#define BUTTON_R      SDLK_c
#define BUTTON_L2     SDLK_f
#define BUTTON_R2     SDLK_v
#define BUTTON_L3     SDLK_g
#define BUTTON_R3     SDLK_h
#define BUTTON_SELECT SDLK_TAB
#define BUTTON_START  SDLK_RETURN

#define SET_SLOT_0    SDLK_0
#define SET_SLOT_1    SDLK_1
#define SET_SLOT_2    SDLK_2
#define SET_SLOT_3    SDLK_3
#define SET_SLOT_4    SDLK_4
#define SET_SLOT_5    SDLK_5
#define SET_SLOT_6    SDLK_6
#define SET_SLOT_7    SDLK_7
#define SET_SLOT_8    SDLK_8
#define SET_SLOT_9    SDLK_9
#define SAVE_STATE    SDLK_F5
#define LOAD_STATE    SDLK_F8

#define PAUSE         SDLK_p
#define FF_TOGGLE     SDLK_MINUS
#define FF_HOLD       SDLK_EQUALS
#define STEP          SDLK_SEMICOLON

bool KeyBinds::init(libretro::LoggerComponent* logger)
{
  _logger = logger;

  _ff = false;

  return true;
}

KeyBinds::Action KeyBinds::translate(const SDL_KeyboardEvent* key, unsigned* extra)
{
  if ((key->keysym.mod & (KMOD_SHIFT | KMOD_CTRL | KMOD_ALT)) == 0)
  {
    if (key->state == SDL_PRESSED && !key->repeat)
    {
      switch (key->keysym.sym)
      {
      case SET_SLOT_0: *extra = 0; return Action::kSetStateSlot;
      case SET_SLOT_1: *extra = 1; return Action::kSetStateSlot;
      case SET_SLOT_2: *extra = 2; return Action::kSetStateSlot;
      case SET_SLOT_3: *extra = 3; return Action::kSetStateSlot;
      case SET_SLOT_4: *extra = 4; return Action::kSetStateSlot;
      case SET_SLOT_5: *extra = 5; return Action::kSetStateSlot;
      case SET_SLOT_6: *extra = 6; return Action::kSetStateSlot;
      case SET_SLOT_7: *extra = 7; return Action::kSetStateSlot;
      case SET_SLOT_8: *extra = 8; return Action::kSetStateSlot;
      case SET_SLOT_9: *extra = 9; return Action::kSetStateSlot;
      case SAVE_STATE: return Action::kSaveState;
      case LOAD_STATE: return Action::kLoadState;

      case PAUSE:      return Action::kPauseToggle;
      case FF_TOGGLE:  _ff = !_ff; *extra = (unsigned)_ff; return Action::kFastForward;
      case FF_HOLD:    *extra = (unsigned)!_ff; return Action::kFastForward;
      case STEP:       return Action::kStep;
      }

      *extra = 1;
    }
    else if (key->state == SDL_RELEASED)
    {
      switch (key->keysym.sym)
      {
      case FF_HOLD: *extra = (unsigned)_ff; return Action::kFastForward;
      }

      *extra = 0;
    }
    else
    {
      return Action::kNothing;
    }

    switch (key->keysym.sym)
    {
    case BUTTON_UP:     return Action::kButtonUp;
    case BUTTON_DOWN:   return Action::kButtonDown;
    case BUTTON_LEFT:   return Action::kButtonLeft;
    case BUTTON_RIGHT:  return Action::kButtonRight;
    case BUTTON_X:      return Action::kButtonX;
    case BUTTON_Y:      return Action::kButtonY;
    case BUTTON_A:      return Action::kButtonA;
    case BUTTON_B:      return Action::kButtonB;
    case BUTTON_L:      return Action::kButtonL;
    case BUTTON_R:      return Action::kButtonR;
    case BUTTON_L2:     return Action::kButtonL2;
    case BUTTON_R2:     return Action::kButtonR2;
    case BUTTON_L3:     return Action::kButtonL3;
    case BUTTON_R3:     return Action::kButtonR3;
    case BUTTON_SELECT: return Action::kButtonSelect;
    case BUTTON_START:  return Action::kButtonStart;
    }
  }

  return Action::kNothing;
}
