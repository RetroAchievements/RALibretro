#pragma once

#include "Emulator.h"
#include <string>
typedef const std::string const_string;

class Application;

class Fsm {
public:
  enum class State {
    GameRunning,
    CoreLoaded,
    GameTurbo,
    GamePaused,
    FrameStep,
    Start,
    Quit,
  };

  Fsm(Application& slave): ctx(slave), __state(State::Start) {}

  State currentState() const { return __state; }

  bool pauseGame();
  bool unloadGame();
  bool turbo();
  bool resetGame();
  bool quit();
  bool loadGame(const_string path);
  bool unloadCore();
  bool normal();
  bool resumeGame();
  bool loadCore(Emulator core);

protected:
  Application& ctx;
  State __state;
};
