#pragma once

// Generated with FSM compiler, https://github.com/leiradel/ddlt


  #include "Emulator.h"
  #include <string>

  typedef const std::string const_string;

class Application;

class Fsm {
public:
  enum class State {
    CoreLoaded,
    FrameStep,
    GamePaused,
    GameRunning,
    GameTurbo,
    Quit,
    Start,
  };

  Fsm(Application& slave): ctx(slave), __state(State::Start) {}

  State currentState() const { return __state; }

#ifdef DEBUG_FSM
  const char* getStateName(State state) const;
#endif

  bool loadCore(Emulator core);
  bool loadGame(const_string path);
  bool loadRecent(Emulator core, const_string path);
  bool normal();
  bool pauseGame();
  bool quit();
  bool resetGame();
  bool resumeGame();
  bool step();
  bool turbo();
  bool unloadCore();
  bool unloadGame();

protected:
  bool before() const;
  bool before(State state) const;
  void after() const;
  void after(State state) const;

  Application& ctx;
  State __state;
};
