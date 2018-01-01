#include "Input.h"

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
  _ports = 0;
  _updated = false;

  ControllerType ctrl;
  ctrl._description = "None";
  ctrl._id = RETRO_DEVICE_NONE;

  for (unsigned i = 0; i < sizeof(_ids) / sizeof(_ids[0]); i++)
  {
    _ids[i].push_back(ctrl);
  }

  // Add controller mappings
  for (size_t i = 0; i < sizeof(s_gameControllerDB) / sizeof(s_gameControllerDB[0]); i++ )
  {
    SDL_GameControllerAddMapping(s_gameControllerDB[i]);
  }

  // Add controllers already connected
  int max = SDL_NumJoysticks();

  for(int i = 0; i < max; i++)
  {
    addController(i);
  }

  return true;
}

void Input::reset()
{
  _descriptors.clear();
  _controllers.clear();

  _ports = 0;

  ControllerType ctrl;
  ctrl._description = "None";
  ctrl._id = RETRO_DEVICE_NONE;

  for (unsigned i = 0; i < sizeof(_ids) / sizeof(_ids[0]); i++)
  {
    _ids[i].clear();
    _ids[i].push_back(ctrl);
  }

  for (auto it = _pads.begin(); it != _pads.end(); ++it)
  {
    Pad* pad = &it->second;

    pad->_port = 0;
    pad->_devId = RETRO_DEVICE_NONE;
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
      _logger->printf(RETRO_LOG_ERROR, "Error opening the controller: %s", SDL_GetError());
      return;
    }

    pad._joystick = SDL_GameControllerGetJoystick(pad._controller);

    if (pad._joystick == NULL)
    {
      _logger->printf(RETRO_LOG_ERROR, "Error getting the joystick: %s", SDL_GetError());
      SDL_GameControllerClose(pad._controller);
      return;
    }

    pad._id = SDL_JoystickInstanceID(pad._joystick);

    if (_pads.find(pad._id) == _pads.end())
    {
      pad._controllerName = SDL_GameControllerName(pad._controller);
      pad._joystickName = SDL_JoystickName(pad._joystick);
      pad._lastDir[0] = pad._lastDir[1] = pad._lastDir[2] =
      pad._lastDir[3] = pad._lastDir[4] = pad._lastDir[5] = -1;
      pad._sensitivity = 0.5f;
      pad._port = 0;
      pad._devId = RETRO_DEVICE_NONE;
      memset(pad._state, 0, sizeof(pad._state));

      _pads.insert(std::make_pair(pad._id, pad));
      _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) added", pad._controllerName, pad._joystickName);
    }
    else
    {
      SDL_GameControllerClose(pad._controller);
    }
  }
}

void Input::setDefaultController()
{
  if (_pads.size() == 0)
  {
    return;
  }

  Pad* pad = &_pads[0];
  uint64_t bit = 1;

  for (unsigned i = 0; i < 64; i++, bit <<= 1)
  {
    if ((_ports & bit) != 0)
    {
      pad->_port = i + 1;

      if (_controllers.size() != 0)
      {
        Controller* ctrl = &_controllers[i];

        for (auto it = ctrl->_types.begin(); it != ctrl->_types.end(); ++it)
        {
          ControllerType* type = &*it;

          if ((type->_id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
          {
            pad->_devId = type->_id;
            break;
          }
        }
      }
      else
      {
        pad->_devId = RETRO_DEVICE_JOYPAD;
      }

      _updated = true;
      return;
    }
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
    desc._id = descs->id;
    desc._description = descs->description;

    _descriptors.push_back(desc);

    unsigned port = desc._port;

    if (port < sizeof(_ids) / sizeof(_ids[0]) && (_ports & (1ULL << port)) == 0)
    {
      ControllerType ctrl;
      ctrl._description = "RetroPad";
      ctrl._id = RETRO_DEVICE_JOYPAD;

      _ids[port].push_back(ctrl);
      _ports |= 1ULL << port;
    }
  }
}

void Input::setControllerInfo(const struct retro_controller_info* info, unsigned count)
{
  for (unsigned i = 0; i < count; info++, i++)
  {
    Controller ctrl;

    for (unsigned j = 0; j < info->num_types; j++)
    {
      ControllerType type;
      type._description = info->types[j].desc;
      type._id = info->types[j].id;

      ctrl._types.push_back(type);

      if ((type._id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
      {
        unsigned port = i;

        if (port < sizeof(_ids) / sizeof(_ids[0]))
        {
          bool found = false;

          for (auto it = _ids[port].begin(); it != _ids[port].end(); ++it)
          {
            if (it->_id == type._id)
            {
              // Overwrite the generic RetroPad description with the one from the controller info
              it->_description = type._description;
              found = true;
              break;
            }
          }

          if (!found)
          {
            _ids[port].push_back(type);
          }

          _ports |= 1ULL << port;
        }
      }
    }

    _controllers.push_back(ctrl);
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
  port++;

  for (auto it = _pads.begin(); it != _pads.end(); ++it)
  {
    Pad* pad = &it->second;

    if (pad->_port == (int)port)
    {
      return pad->_devId;
    }
  }

  // The controller was removed
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

  port++;

  for (auto it = _pads.begin(); it != _pads.end(); ++it)
  {
    Pad* pad = &it->second;

    if (pad->_port == (int)port && pad->_devId == device)
    {
      return pad->_state[id] ? 32767 : 0;
    }
  }

  return 0;
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
    _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) removed", pad->_controllerName, pad->_joystickName);
    SDL_GameControllerClose(pad->_controller);
    _pads.erase(it);

    // Flag a pending update so the core will receive an event for this removal
    _updated = true;
  }
}

void Input::controllerButton(const SDL_Event* event)
{
  auto it = _pads.find(event->cbutton.which);

  if (it != _pads.end())
  {
    Pad* pad = &it->second;
    unsigned button;

    switch (event->cbutton.button)
    {
    case SDL_CONTROLLER_BUTTON_A:             button = RETRO_DEVICE_ID_JOYPAD_B; break;
    case SDL_CONTROLLER_BUTTON_B:             button = RETRO_DEVICE_ID_JOYPAD_A; break;
    case SDL_CONTROLLER_BUTTON_X:             button = RETRO_DEVICE_ID_JOYPAD_Y; break;
    case SDL_CONTROLLER_BUTTON_Y:             button = RETRO_DEVICE_ID_JOYPAD_X; break;
    case SDL_CONTROLLER_BUTTON_BACK:          button = RETRO_DEVICE_ID_JOYPAD_SELECT; break;
    case SDL_CONTROLLER_BUTTON_START:         button = RETRO_DEVICE_ID_JOYPAD_START; break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     button = RETRO_DEVICE_ID_JOYPAD_L3; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    button = RETRO_DEVICE_ID_JOYPAD_R3; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  button = RETRO_DEVICE_ID_JOYPAD_L; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: button = RETRO_DEVICE_ID_JOYPAD_R; break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       button = RETRO_DEVICE_ID_JOYPAD_UP; break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     button = RETRO_DEVICE_ID_JOYPAD_DOWN; break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     button = RETRO_DEVICE_ID_JOYPAD_LEFT; break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    button = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
    case SDL_CONTROLLER_BUTTON_GUIDE:         // fallthrough
    default:                                  return;
    }

    pad->_state[button] = event->cbutton.state == SDL_PRESSED;
  }
}

void Input::controllerAxis(const SDL_Event* event)
{
  auto it = _pads.find(event->caxis.which);

  if (it != _pads.end())
  {
    Pad* pad = &it->second;
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
