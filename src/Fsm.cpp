#include "Fsm.h"
#include "Application.h"

bool Fsm::before() const {
  return true;
}

bool Fsm::before(State state) const {
  switch (state) {
    default: break;
  }

  return true;
}

void Fsm::after() const {

    ctx.updateMenu();
  
}

void Fsm::after(State state) const {
  switch (state) {
    default: break;
  }
}

bool Fsm::loadCore(Emulator core) {
  switch (__state) {
    case State::Start: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (!ctx.loadCore(core)) {
        return false;
      }
    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to CoreLoaded");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::loadGame(const_string path) {
  switch (__state) {
    case State::CoreLoaded: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (!ctx.loadGame(path)) {
        return false;
      }
    
      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameRunning");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::normal() {
  switch (__state) {
    case State::GameTurbo: {
      if (!before() || !before(__state)) {
        return false;
      }

      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameRunning");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::pauseGame() {
  switch (__state) {
    case State::GameRunning: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (ctx.hardcore()) {
        return false;
      }

      ctx.pauseGame(true);
    
      __state = State::GamePaused;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GamePaused");
#endif
      return true;
    }
    break;

    case State::GameTurbo: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (ctx.hardcore()) {
        return false;
      }

      ctx.pauseGame(true);
    
      __state = State::GamePaused;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GamePaused");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::quit() {
  switch (__state) {
    case State::CoreLoaded: {
      if (!before() || !before(__state)) {
        return false;
      }

      bool __ok = unloadCore() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to Quit");
#endif
      }

      return __ok;
    }
    break;

    case State::GamePaused: {
      if (!before() || !before(__state)) {
        return false;
      }

      bool __ok = unloadGame() &&
                  unloadCore() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to Quit");
#endif
      }

      return __ok;
    }
    break;

    case State::GameRunning: {
      if (!before() || !before(__state)) {
        return false;
      }

      bool __ok = unloadGame() &&
                  unloadCore() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to Quit");
#endif
      }

      return __ok;
    }
    break;

    case State::GameTurbo: {
      if (!before() || !before(__state)) {
        return false;
      }

      bool __ok = unloadGame() &&
                  unloadCore() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to Quit");
#endif
      }

      return __ok;
    }
    break;

    case State::Start: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (!ctx.canQuit()) {
        return false;
      }
    
      __state = State::Quit;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to Quit");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::resetGame() {
  switch (__state) {
    case State::GamePaused: {
      if (!before() || !before(__state)) {
        return false;
      }

      bool __ok = resumeGame() &&
                  resetGame();

      if (__ok) {
        after(__state);
        after();

#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to GameRunning");
#endif
      }

      return __ok;
    }
    break;

    case State::GameRunning: {
      if (!before() || !before(__state)) {
        return false;
      }


      ctx.resetGame();
    
      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameRunning");
#endif
      return true;
    }
    break;

    case State::GameTurbo: {
      if (!before() || !before(__state)) {
        return false;
      }

      bool __ok = normal() &&
                  resetGame();

      if (__ok) {
        after(__state);
        after();

#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to GameRunning");
#endif
      }

      return __ok;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::resumeGame() {
  switch (__state) {
    case State::FrameStep: {
      if (!before() || !before(__state)) {
        return false;
      }

      __state = State::GamePaused;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GamePaused");
#endif
      return true;
    }
    break;

    case State::GamePaused: {
      if (!before() || !before(__state)) {
        return false;
      }


      ctx.pauseGame(false);
    
      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameRunning");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::step() {
  switch (__state) {
    case State::GamePaused: {
      if (!before() || !before(__state)) {
        return false;
      }

      __state = State::FrameStep;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to FrameStep");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::turbo() {
  switch (__state) {
    case State::GamePaused: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (ctx.hardcore()) {
        return false;
      }
    
      __state = State::GameTurbo;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameTurbo");
#endif
      return true;
    }
    break;

    case State::GameRunning: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (ctx.hardcore()) {
        return false;
      }
    
      __state = State::GameTurbo;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameTurbo");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::unloadCore() {
  switch (__state) {
    case State::CoreLoaded: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (!ctx.unloadCore()) {
        return false;
      }
    
      __state = State::Start;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to Start");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::unloadGame() {
  switch (__state) {
    case State::GamePaused: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (!ctx.unloadGame()) {
        return false;
      }
    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to CoreLoaded");
#endif
      return true;
    }
    break;

    case State::GameRunning: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (!ctx.unloadGame()) {
        return false;
      }
    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to CoreLoaded");
#endif
      return true;
    }
    break;

    case State::GameTurbo: {
      if (!before() || !before(__state)) {
        return false;
      }


      if (!ctx.unloadGame()) {
        return false;
      }
    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to CoreLoaded");
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

