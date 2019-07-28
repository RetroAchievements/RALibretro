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

#include "Input.h"

#include "Dialog.h"
#include "jsonsax/jsonsax.h"

#include <SDL_hints.h>

#define KEYBOARD_ID -1

#define TAG "[INP] "

static const char* s_gameControllerDB[] =
{
  // Updated on 2017-06-15
  #include "GameControllerDB.inl"
  // Some controllers I have around
  "63252305000000000000504944564944,USB Vibration Joystick (BM),platform:Windows,x:b3,a:b2,b:b1,y:b0,back:b8,start:b9,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,dpup:h0.1,leftshoulder:b4,lefttrigger:b6,rightshoulder:b5,righttrigger:b7,leftstick:b10,rightstick:b11,leftx:a0,lefty:a1,rightx:a2,righty:a3,",
  "351212ab000000000000504944564944,NES30 Joystick,platform:Windows,x:b3,a:b2,b:b1,y:b0,back:b6,start:b7,leftshoulder:b4,rightshoulder:b5,leftx:a0,lefty:a1,"
};

bool Input::init(libretro::LoggerComponent* logger)
{
  _logger = logger;

  reset();

  // Add controller mappings
  for (size_t i = 0; i < sizeof(s_gameControllerDB) / sizeof(s_gameControllerDB[0]); i++ )
  {
    SDL_GameControllerAddMapping(s_gameControllerDB[i]);
  }

  // Add the keyboard controller
  Pad keyb;

  keyb._id = KEYBOARD_ID;
  keyb._controller = NULL;
  keyb._controllerName = "Keyboard";
  keyb._joystick = NULL;
  keyb._joystickName = "Keyboard";
  keyb._ports = 0;
  keyb._lastDir[0] = keyb._lastDir[1] = keyb._lastDir[2] =
  keyb._lastDir[3] = keyb._lastDir[4] = keyb._lastDir[5] = -1;
  keyb._sensitivity = 0.5f;
  memset(keyb._state, 0, sizeof(keyb._state));

  _pads.insert(std::make_pair(keyb._id, keyb));
  _logger->info(TAG "Controller %s (%s) added", keyb._controllerName, keyb._joystickName);

  // Add controllers already connected
  int max = SDL_NumJoysticks();

  for(int i = 0; i < max; i++)
  {
    addController(i);
  }

  if (max > 0)
  {
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_B] = { 0, SDL_CONTROLLER_BUTTON_B, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_Y] = { 0, SDL_CONTROLLER_BUTTON_Y, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_SELECT] = { 0, SDL_CONTROLLER_BUTTON_BACK, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_START] = { 0, SDL_CONTROLLER_BUTTON_START, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_UP] = { 0, SDL_CONTROLLER_BUTTON_DPAD_UP, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_DOWN] = { 0, SDL_CONTROLLER_BUTTON_DPAD_DOWN, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_LEFT] = { 0, SDL_CONTROLLER_BUTTON_DPAD_LEFT, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_RIGHT] = { 0, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_A] = { 0, SDL_CONTROLLER_BUTTON_A, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_X] = { 0, SDL_CONTROLLER_BUTTON_X, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_L] = { 0, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_R] = { 0, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_L2] = { 0, SDL_CONTROLLER_AXIS_TRIGGERLEFT, ButtonDescriptor::Type::Axis, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_R2] = { 0, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, ButtonDescriptor::Type::Axis, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_L3] = { 0, SDL_CONTROLLER_BUTTON_LEFTSTICK, ButtonDescriptor::Type::Button, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_R3] = { 0, SDL_CONTROLLER_BUTTON_RIGHTSTICK, ButtonDescriptor::Type::Button, 0 };
  }
  else
  {
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_B] = { 0, SDLK_z, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_Y] = { 0, SDLK_a, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_SELECT] = { 0, SDLK_TAB, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_START] = { 0, SDLK_RETURN, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_UP] = { 0, SDLK_UP, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_DOWN] = { 0, SDLK_DOWN, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_LEFT] = { 0, SDLK_LEFT, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_RIGHT] = { 0, SDLK_RIGHT, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_A] = { 0, SDLK_x, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_X] = { 0, SDLK_s, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_L] = { 0, SDLK_d, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_R] = { 0, SDLK_c, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_L2] = { 0, SDLK_f, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_R2] = { 0, SDLK_v, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_L3] = { 0, SDLK_g, ButtonDescriptor::Type::Key, 0 };
    _buttonMap[RETRO_DEVICE_ID_JOYPAD_R3] = { 0, SDLK_h, ButtonDescriptor::Type::Key, 0 };
  }

  return true;
}

void Input::reset()
{
  _descriptors.clear();

  _ports = 0;
  _updated = false;

  ControllerInfo none;
  none._description = "None";
  none._id = RETRO_DEVICE_NONE;

  ControllerInfo retropad;
  retropad._description = "RetroPad";
  retropad._id = RETRO_DEVICE_JOYPAD;

  for (unsigned i = 0; i < kMaxPorts; i++)
  {
    _info[i].clear();
    _info[i].push_back(none);
    _info[i].push_back(retropad);

    _devices[i] = 0;
  }

  for (auto& pair: _pads)
  {
    Pad* pad = &pair.second;
    pad->_ports = 0;
  }
}

void Input::addController(int which)
{
  if (SDL_IsGameController(which))
  {
    Pad pad;

    pad._controller = SDL_GameControllerOpen(which);

    if (pad._controller == NULL)
    {
      _logger->error(TAG "Error opening the controller: %s", SDL_GetError());
      return;
    }

    pad._joystick = SDL_GameControllerGetJoystick(pad._controller);

    if (pad._joystick == NULL)
    {
      _logger->error(TAG "Error getting the joystick: %s", SDL_GetError());
      SDL_GameControllerClose(pad._controller);
      return;
    }

    pad._id = SDL_JoystickInstanceID(pad._joystick);

    if (_pads.find(pad._id) != _pads.end())
    {
      SDL_GameControllerClose(pad._controller);
      return;
    }

    pad._controllerName = SDL_GameControllerName(pad._controller);
    pad._joystickName = SDL_JoystickName(pad._joystick);
    pad._ports = 0;
    pad._lastDir[0] = pad._lastDir[1] = pad._lastDir[2] =
    pad._lastDir[3] = pad._lastDir[4] = pad._lastDir[5] = -1;
    pad._sensitivity = 0.5f;
    memset(pad._state, 0, sizeof(pad._state));

    _pads.insert(std::make_pair(pad._id, pad));
    _logger->info(TAG "Controller %s (%s) added", pad._controllerName, pad._joystickName);
  }
}

void Input::autoAssign()
{
  Pad* pad = NULL;

  for (auto& pair: _pads)
  {
    if (pair.second._id != KEYBOARD_ID)
    {
      pad = &pair.second;
      break;
    }
  }
  
  if (pad == NULL)
  {
    auto found = _pads.find(KEYBOARD_ID);

    if (found != _pads.end())
    {
      pad = &found->second;
    }
  }

  if (pad == NULL)
  {
    return;
  }

  uint64_t bit = 1;

  for (unsigned port = 0; port < kMaxPorts; port++, bit <<= 1)
  {
    if ((_ports & bit) != 0)
    {
      pad->_ports |= bit;

      if (_devices[port] == 0)
      {
        for (unsigned i = 1 /* Skip None */; i < _info[port].size(); i++)
        {
          const ControllerInfo* info = &_info[port][i];

          if ((info->_id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
          {
            _devices[port] = i;
            break;
          }
        }
      }

      _updated = true;
    }
  }
}

void Input::buttonEvent(Button button, bool pressed)
{
  auto found = _pads.find(KEYBOARD_ID);

  if (found != _pads.end())
  {
    Pad* pad = &found->second;
    unsigned rbutton;

    switch (button)
    {
    case Button::kUp:     rbutton = RETRO_DEVICE_ID_JOYPAD_UP; break;
    case Button::kDown:   rbutton = RETRO_DEVICE_ID_JOYPAD_DOWN; break;
    case Button::kLeft:   rbutton = RETRO_DEVICE_ID_JOYPAD_LEFT; break;
    case Button::kRight:  rbutton = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
    case Button::kX:      rbutton = RETRO_DEVICE_ID_JOYPAD_X; break;
    case Button::kY:      rbutton = RETRO_DEVICE_ID_JOYPAD_Y; break;
    case Button::kA:      rbutton = RETRO_DEVICE_ID_JOYPAD_A; break;
    case Button::kB:      rbutton = RETRO_DEVICE_ID_JOYPAD_B; break;
    case Button::kL:      rbutton = RETRO_DEVICE_ID_JOYPAD_L; break;
    case Button::kR:      rbutton = RETRO_DEVICE_ID_JOYPAD_R; break;
    case Button::kL2:     rbutton = RETRO_DEVICE_ID_JOYPAD_L2; break;
    case Button::kR2:     rbutton = RETRO_DEVICE_ID_JOYPAD_R2; break;
    case Button::kL3:     rbutton = RETRO_DEVICE_ID_JOYPAD_L3; break;
    case Button::kR3:     rbutton = RETRO_DEVICE_ID_JOYPAD_R3; break;
    case Button::kSelect: rbutton = RETRO_DEVICE_ID_JOYPAD_SELECT; break;
    case Button::kStart:  rbutton = RETRO_DEVICE_ID_JOYPAD_START; break;
    default:              return;
    }

    pad->_state[rbutton] = pressed;
  }
}

void Input::processEvent(const SDL_Event* event)
{
  switch (event->type)
  {
  case SDL_CONTROLLERDEVICEADDED:
    addController(event);
    break;

  case SDL_CONTROLLERDEVICEREMOVED:
    removeController(event);
    break;

  case SDL_CONTROLLERBUTTONUP:
  case SDL_CONTROLLERBUTTONDOWN:
    controllerButton(event);
    break;

  case SDL_CONTROLLERAXISMOTION:
    controllerAxis(event);
    break;
  }
}

void Input::setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count)
{
  for (unsigned i = 0; i < count; descs++, i++)
  {
    Descriptor desc;
    desc._port = descs->port;
    desc._device = descs->device;
    desc._index = descs->index;
    desc._button = descs->id;
    desc._description = descs->description;

    _descriptors.push_back(desc);

    if (desc._port < kMaxPorts)
    {
      _ports |= 1ULL << desc._port;
    }
    else
    {
      _logger->warn(TAG "Port %u above %d limit", desc._port + 1, kMaxPorts);
    }
  }
}

void Input::setControllerInfo(const struct retro_controller_info* rinfo, unsigned count)
{
  for (unsigned port = 0; port < count; rinfo++, port++)
  {
    for (unsigned i = 0; i < rinfo->num_types; i++)
    {
      ControllerInfo info;
      info._description = rinfo->types[i].desc;
      info._id = rinfo->types[i].id;

      if ((info._id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
      {
        if (port < kMaxPorts)
        {
          _info[port].push_back(info);
          _ports |= 1ULL << port;
        }
        else
        {
          _logger->warn(TAG "Port %u above %d limit", port + 1, kMaxPorts);
        }
      }
    }
  }
}

bool Input::ctrlUpdated()
{
  bool updated = _updated;
  _updated = false;
  return updated;
}

unsigned Input::getController(unsigned port)
{
  uint64_t bit = 1ULL << port;

  for (const auto& pair: _pads)
  {
    const Pad* pad = &pair.second;

    if ((pad->_ports & bit) != 0)
    {
      return _info[port][_devices[port]]._id;
    }
  }

  return RETRO_DEVICE_NONE;
}

void Input::poll()
{
  // Events are polled in the main event loop, and arrive in this class via
  // the processEvent method
}

int16_t Input::read(unsigned port, unsigned device, unsigned index, unsigned id)
{
  (void)index;

  uint64_t bit = 1ULL << port;

  for (const auto& pair: _pads)
  {
    const Pad* pad = &pair.second;

    if ((pad->_ports & bit) != 0 && _devices[port] != 0)
    {
      return pad->_state[id] ? 32767 : 0;
    }
  }

  return 0;
}

Input::ButtonDescriptor Input::captureButtonPress()
{
  Input::ButtonDescriptor desc = { 0, 0, Input::ButtonDescriptor::Type::None, 0 };

  if (!_pads.empty())
  {
    SDL_GameControllerUpdate();
    for (const auto& pair : _pads)
    {
      if (!pair.second._controller)
        continue;

      for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
      {
        if (SDL_GameControllerGetButton(pair.second._controller, static_cast<SDL_GameControllerButton>(i)))
        {
          desc.joystick_id = pair.second._id;
          desc.type = Input::ButtonDescriptor::Type::Button;
          desc.button = i;
          return desc;
        }
      }

      int threshold = 32767 * pair.second._sensitivity;

      for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
      {
        const auto value = SDL_GameControllerGetAxis(pair.second._controller, static_cast<SDL_GameControllerAxis>(i));
        if (value < -threshold)
        {
          desc.joystick_id = pair.second._id;
          desc.type = Input::ButtonDescriptor::Type::Axis;
          desc.button = i;
          desc.modifiers = 0xFF;
          return desc;
        }
        else if (value > threshold)
        {
          desc.joystick_id = pair.second._id;
          desc.type = Input::ButtonDescriptor::Type::Axis;
          desc.button = i;
          if (i != SDL_CONTROLLER_AXIS_TRIGGERLEFT && i != SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            desc.modifiers = 1;
          return desc;
        }
      }
    }
  }

  return desc;
}

void Input::addController(const SDL_Event* event)
{
  addController(event->cdevice.which);
}

void Input::removeController(const SDL_Event* event)
{
  auto it = _pads.find(event->cdevice.which);

  if (it != _pads.end())
  {
    Pad* pad = &it->second;

    SDL_GameControllerClose(pad->_controller);
    _pads.erase(it);

    _logger->info(TAG "Controller %s (%s) removed", pad->_controllerName, pad->_joystickName);

    // Flag a pending update so the core will receive an event for this removal
    _updated = true;
  }
}

void Input::controllerButton(const SDL_Event* event)
{
  auto found = _pads.find(event->cbutton.which);

  if (found != _pads.end())
  {
    Pad* pad = &found->second;
    unsigned button;

    switch (event->cbutton.button)
    {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       button = RETRO_DEVICE_ID_JOYPAD_UP; break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     button = RETRO_DEVICE_ID_JOYPAD_DOWN; break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     button = RETRO_DEVICE_ID_JOYPAD_LEFT; break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    button = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
    case SDL_CONTROLLER_BUTTON_Y:             button = RETRO_DEVICE_ID_JOYPAD_X; break;
    case SDL_CONTROLLER_BUTTON_X:             button = RETRO_DEVICE_ID_JOYPAD_Y; break;
    case SDL_CONTROLLER_BUTTON_B:             button = RETRO_DEVICE_ID_JOYPAD_A; break;
    case SDL_CONTROLLER_BUTTON_A:             button = RETRO_DEVICE_ID_JOYPAD_B; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  button = RETRO_DEVICE_ID_JOYPAD_L; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: button = RETRO_DEVICE_ID_JOYPAD_R; break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     button = RETRO_DEVICE_ID_JOYPAD_L3; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    button = RETRO_DEVICE_ID_JOYPAD_R3; break;
    case SDL_CONTROLLER_BUTTON_BACK:          button = RETRO_DEVICE_ID_JOYPAD_SELECT; break;
    case SDL_CONTROLLER_BUTTON_START:         button = RETRO_DEVICE_ID_JOYPAD_START; break;
    case SDL_CONTROLLER_BUTTON_GUIDE:         // fallthrough
    default:                                  return;
    }

    pad->_state[button] = event->cbutton.state == SDL_PRESSED;
  }
}

void Input::controllerAxis(const SDL_Event* event)
{
  auto found = _pads.find(event->caxis.which);

  if (found != _pads.end())
  {
    Pad* pad = &found->second;
    int threshold = 32767 * pad->_sensitivity;
    int positive, negative;
    int button;
    int *last_dir;

    switch (event->caxis.axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX:
    case SDL_CONTROLLER_AXIS_LEFTY:
    case SDL_CONTROLLER_AXIS_RIGHTX:
    case SDL_CONTROLLER_AXIS_RIGHTY:
      switch (event->caxis.axis)
      {
      case SDL_CONTROLLER_AXIS_LEFTX:
        positive = RETRO_DEVICE_ID_JOYPAD_RIGHT;
        negative = RETRO_DEVICE_ID_JOYPAD_LEFT;
        last_dir = pad->_lastDir + 0;
        break;

      case SDL_CONTROLLER_AXIS_LEFTY:
        positive = RETRO_DEVICE_ID_JOYPAD_DOWN;
        negative = RETRO_DEVICE_ID_JOYPAD_UP;
        last_dir = pad->_lastDir + 1;
        break;

      case SDL_CONTROLLER_AXIS_RIGHTX:
        positive = RETRO_DEVICE_ID_JOYPAD_RIGHT;
        negative = RETRO_DEVICE_ID_JOYPAD_LEFT;
        last_dir = pad->_lastDir + 2;
        break;

      case SDL_CONTROLLER_AXIS_RIGHTY:
        positive = RETRO_DEVICE_ID_JOYPAD_DOWN;
        negative = RETRO_DEVICE_ID_JOYPAD_UP;
        last_dir = pad->_lastDir + 3;
        break;
      }

      if (event->caxis.value < -threshold)
      {
        button = negative;
      }
      else if (event->caxis.value > threshold)
      {
        button = positive;
      }
      else
      {
        button = -1;
      }

      break;

    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
      if (event->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
      {
        button = RETRO_DEVICE_ID_JOYPAD_L2;
        last_dir = pad->_lastDir + 4;
      }
      else
      {
        button = RETRO_DEVICE_ID_JOYPAD_R2;
        last_dir = pad->_lastDir + 5;
      }

      break;

    default:
      return;
    }

    if (*last_dir != -1)
    {
      pad->_state[*last_dir] = false;
    }

    if (event->caxis.value < -threshold || event->caxis.value > threshold)
    {
      pad->_state[button] = true;
    }

    *last_dir = button;
  }
}

std::string Input::serialize()
{
  std::string json("[");
  const char* comma = "";

  for (unsigned port = 0; port < kMaxPorts; port++)
  {
    char temp[128];
    snprintf(temp, sizeof(temp), "%s{\"port\":%u,\"device\":%d}", comma, port + 1, _devices[port]);
    json.append(temp);
    comma = ",";
  }

  json.append("]");
  return json;
}

void Input::deserialize(const char* json)
{
  struct Deserialize
  {
    Input* self;
    std::string key;
    unsigned port;
  };

  Deserialize ud;
  ud.self = this;

  jsonsax_parse(json, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num) {
    auto ud = (Deserialize*)udata;

    if (event == JSONSAX_KEY)
    {
      ud->key = std::string(str, num);
    }
    else if (event == JSONSAX_NUMBER)
    {
      if (ud->key == "port")
      {
        ud->port = strtoul(str, NULL, 10) - 1;
      }
      else if (ud->key == "device")
      {
        ud->self->_devices[ud->port] = strtol(str, NULL, 10);
      }
    }
    
    return 0;
  });
}

void Input::showDialog()
{
  int selectedDevice[kMaxPorts];
  int selectedPad[kMaxPorts];

  for (unsigned port = 0; port < kMaxPorts; port++)
  {
    selectedDevice[port] = _devices[port];

    uint64_t bit = 1ULL << port;
    int selected = 0;
    selectedPad[port] = 0;

    for (const auto& pair: _pads)
    {
      selected++;
      const Pad* pad = &pair.second;

      if ((pad->_ports & bit) != 0)
      {
        selectedPad[port] = selected;
        break;
      }
    }
  }

  const WORD WIDTH = 280;
  const WORD LINE = 15;

  Dialog db;
  db.init("Input Settings");

  WORD y = 0;
  DWORD id = 0;

  for (unsigned port = 0; port < kMaxPorts; port++)
  {
    char label[32];
    snprintf(label, sizeof(label), "Port %u", port + 1);
    db.addLabel(label, 0, y, WIDTH / 3 - 5, 8);

    db.addCombobox(40000 + id, WIDTH / 3 + 5, y, WIDTH / 3 - 10, LINE, 5, s_getType, (void*)&_info[port], &selectedDevice[port]);

    db.addCombobox(50000 + id, WIDTH * 2 / 3 + 5, y, WIDTH / 3 - 5, LINE, 5, s_getPad, (void*)&_pads, &selectedPad[port]);

    y += LINE;
    id++;
  }

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  _updated = db.show();

  if (_updated)
  {
    for (auto& pair: _pads)
    {
      Pad* pad = &pair.second;
      pad->_ports = 0;
    }

    for (unsigned port = 0; port < kMaxPorts; port++)
    {
      _devices[port] = selectedDevice[port];

      uint64_t bit = 1ULL << port;
      int selected = selectedPad[port];

      for (auto& pair: _pads)
      {
        Pad* pad = &pair.second;

        if (--selected == 0)
        {
          pad->_ports |= bit;
          break;
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

  const Input::ButtonDescriptor getButtonDescriptor() const { return _buttonDescriptor; }

  static void getButtonLabel(char buffer[32], Input::ButtonDescriptor& desc)
  {
    switch (desc.type)
    {
      case Input::ButtonDescriptor::Type::None:
        sprintf(buffer, "none");
        break;

      case Input::ButtonDescriptor::Type::Button:
        sprintf(buffer, "J%d %s", desc.joystick_id, SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(desc.button)));
        break;

      case Input::ButtonDescriptor::Type::Axis:
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

      case Input::ButtonDescriptor::Type::Key:
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
                  _buttonDescriptor.type = Input::ButtonDescriptor::Key;
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
  Input::ButtonDescriptor _buttonDescriptor;

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
          _buttonDescriptor.type = Input::ButtonDescriptor::Type::None;
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
          Input::ButtonDescriptor button = _input->captureButtonPress();
          if (button.type != Input::ButtonDescriptor::Type::None)
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
  void initButtons(const std::array<Input::ButtonDescriptor, 16>& buttonMap)
  {
    _buttonMap = buttonMap;

    const WORD WIDTH = 360;
    const WORD HEIGHT = 150;

    addButtonInput(0, 1, "L2", RETRO_DEVICE_ID_JOYPAD_L2);
    addButtonInput(0, 9, "R2", RETRO_DEVICE_ID_JOYPAD_R2);
    addButtonInput(1, 1, "L1", RETRO_DEVICE_ID_JOYPAD_L);
    addButtonInput(1, 9, "R1", RETRO_DEVICE_ID_JOYPAD_R);
    addButtonInput(2, 1, "Up", RETRO_DEVICE_ID_JOYPAD_UP);
    addButtonInput(2, 4, "Select", RETRO_DEVICE_ID_JOYPAD_SELECT);
    addButtonInput(2, 6, "Start", RETRO_DEVICE_ID_JOYPAD_START);
    addButtonInput(2, 9, "Y", RETRO_DEVICE_ID_JOYPAD_Y);
    addButtonInput(3, 0, "Left", RETRO_DEVICE_ID_JOYPAD_LEFT);
    addButtonInput(3, 2, "Right", RETRO_DEVICE_ID_JOYPAD_RIGHT);
    addButtonInput(3, 8, "X", RETRO_DEVICE_ID_JOYPAD_X);
    addButtonInput(3, 10, "B", RETRO_DEVICE_ID_JOYPAD_B);
    addButtonInput(4, 1, "Down", RETRO_DEVICE_ID_JOYPAD_DOWN);
    addButtonInput(4, 9, "A", RETRO_DEVICE_ID_JOYPAD_A);

    addButton("OK", IDOK, WIDTH - 55 - 50, HEIGHT - 14, 50, 14, true);
    addButton("Cancel", IDCANCEL, WIDTH - 50, HEIGHT - 14, 50, 14, false);
  }

  Input* _input = nullptr;

protected:
  std::array<Input::ButtonDescriptor, 16> _buttonMap = {};
  std::array<char[32], 16> _buttonLabels = {};

  void updateButtonLabel(int button)
  {
    Input::ButtonDescriptor& desc = _buttonMap[button];
    ChangeInputDialog::getButtonLabel(_buttonLabels[button], desc);
  }

  void addButtonInput(int row, int column, const char* label, int button)
  {
    const WORD ITEM_WIDTH = 60;
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
        if (controlId > 20000 && controlId < 20020)
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
      _buttonMap[button] = db.getButtonDescriptor();
      updateButtonLabel(button);
      SetDlgItemText(hwnd, 10000 + button, _buttonLabels[button]);
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
  }
};

void Input::showControllerDialog(int port)
{
  char label[32];
  snprintf(label, sizeof(label), "Controller %u", port + 1);

  InputDialog db;
  db.init(label);
  db._input = this;
  db.initButtons(_buttonMap);

  if (db.show())
  {
    _updated = true;
  }
}

const char* Input::s_getType(int index, void* udata)
{
  auto info = (std::vector<ControllerInfo>*)udata;

  if ((size_t)index < info->size())
  {
    return info->at(index)._description.c_str();
  }

  return NULL;
}

const char* Input::s_getPad(int index, void* udata)
{
  if (index == 0)
  {
    return "Unassigned";
  }

  auto pads = (std::map<SDL_JoystickID, Pad>*)udata;

  for (const auto& pad: *pads)
  {
    if (--index == 0)
    {
      return pad.second._controllerName;
    }
  }

  return NULL;
}
