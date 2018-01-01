#pragma once

#include "libretro/Components.h"

#include <map>

#include <SDL_events.h>
#include <SDL_joystick.h>

class Input: public libretro::InputComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy() {}
  void reset();

  void addController(int which);
  void setDefaultController();
  void processEvent(const SDL_Event* event);

  virtual void setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) override;

  virtual void     setControllerInfo(const struct retro_controller_info* info, unsigned count) override;
  virtual bool     ctrlUpdated() override;
  virtual unsigned getController(unsigned port) override;

  virtual void    poll() override;
  virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) override;

protected:
  struct Pad
  {
    SDL_JoystickID      _id;
    SDL_GameController* _controller;
    const char*         _controllerName;
    SDL_Joystick*       _joystick;
    const char*         _joystickName;
    int                 _lastDir[6];
    bool                _state[16];
    float               _sensitivity;

    int      _port;
    unsigned _devId;
  };

  struct Descriptor
  {
    unsigned _port;
    unsigned _device;
    unsigned _index;
    unsigned _id;

    std::string _description;
  };

  struct ControllerType
  {
    std::string _description;
    unsigned    _id;
  };

  struct Controller
  {
    std::vector<ControllerType> _types;
  };

  void addController(const SDL_Event* event);
  void removeController(const SDL_Event* event);
  void controllerButton(const SDL_Event* event);
  void controllerAxis(const SDL_Event* event);

  libretro::LoggerComponent* _logger;

  bool _updated;

  std::map<SDL_JoystickID, Pad> _pads;
  std::vector<Descriptor>       _descriptors;
  std::vector<Controller>       _controllers;

  uint64_t                    _ports;
  std::vector<ControllerType> _ids[64];
};
