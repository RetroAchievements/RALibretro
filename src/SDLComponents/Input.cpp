#include "Input.h"

#include "Dialog.h"
#include "jsonsax/jsonsax.h"

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
    _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) added", pad._controllerName, pad._joystickName);
  }
}

void Input::autoAssign()
{
  for (auto& pair: _pads)
  {
    Pad* pad = &pair.second;
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
        return;
      }
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
    desc._button = descs->id;
    desc._description = descs->description;

    _descriptors.push_back(desc);

    if (desc._port < kMaxPorts)
    {
      _ports |= 1ULL << desc._port;
    }
    else
    {
      _logger->printf(RETRO_LOG_WARN, "Port %u above %d limit", desc._port + 1, kMaxPorts);
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
          _logger->printf(RETRO_LOG_WARN, "Port %u above %d limit", port + 1, kMaxPorts);
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

    _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) removed", pad->_controllerName, pad->_joystickName);

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
    snprintf(temp, sizeof(temp), "%s{\"port\":%u,\"device\":%d}", comma, port, _devices[port]);
    json.append(temp);
    comma = ",";
  }

  json.append("]");
  return json;
}

struct Deserialize
{
  Input*   _self;
  bool     _isPort;
  unsigned _port;
};

void Input::deserialize(const char* json)
{
  Deserialize ud;
  ud._self = this;
  
  jsonsax_handlers_t handlers;
  memset(&handlers, 0, sizeof(handlers));
  handlers.key = s_key;
  handlers.number = s_number;

  jsonsax_parse(json, &handlers, &ud);
}

int Input::s_key(void* userdata, const char* name, size_t length)
{
  auto* ud = (Deserialize*)userdata;
  ud->_isPort = length == 4 && !strncmp(name, "port", length);
  return 0;
}

int Input::s_number(void* userdata, const char* number, size_t length)
{
  auto* ud = (Deserialize*)userdata;
  unsigned long n = strtoul(number, NULL, 10);

  if (ud->_isPort)
  {
    ud->_port = n;
  }
  else
  {
    ud->_self->_devices[ud->_port] = n;
  }

  return 0;
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
  db.init("Settings");

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
