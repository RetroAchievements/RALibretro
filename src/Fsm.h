#pragma once

#include "Emulator.h"
#include <string>
typedef const std::string const_string;

class Application;

class Fsm {
public:
  enum class State {
    CoreLoaded,
    Start,
    Quit,
    GamePaused,
    GameTurbo,
    GameRunning,
    FrameStep,
  };

  Fsm(Application& slave): ctx(slave), __state(State::Start) {}

  State currentState() const { return __state; }

  bool loadGame(const_string path);
  bool unloadCore();
  bool quit();
  bool loadCore(Emulator core);
  bool resumeGame();
  bool normal();
  bool pauseGame();
  bool resetGame();
  bool unloadGame();
  bool turbo();

protected:
  Application& ctx;
  State __state;
};
