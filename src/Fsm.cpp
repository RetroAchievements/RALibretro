#include "Fsm.h"
#include "Application.h"

bool Fsm::loadCore(Emulator core) {
  switch (__state) {
    case State::Start: {
      if (ctx.loadCore(core)) {
        __state = State::CoreLoaded;
        ctx.updateMenu();
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to CoreLoaded");
#endif
        return true;
      }
      break;
    }
    default: break;
  }

  return false;
}

bool Fsm::loadGame(const_string path) {
  switch (__state) {
    case State::CoreLoaded: {
      if (ctx.loadGame(path)) {
        __state = State::GameRunning;
        ctx.updateMenu();
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to GameRunning");
#endif
        return true;
      }
      break;
    }
    default: break;
  }

  return false;
}

bool Fsm::normal() {
  switch (__state) {
    case State::GameTurbo: {
      __state = State::GameRunning;
      ctx.updateMenu();
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameRunning");
#endif
      return true;
    }
    default: break;
  }

  return false;
}

bool Fsm::pauseGame() {
  switch (__state) {
    case State::GameRunning: {
      if (!ctx.hardcore()) {
        __state = State::GamePaused;
        ctx.pauseGame(true);
        ctx.updateMenu();
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to GamePaused");
#endif
        return true;
      }
      break;
    }
    case State::GameTurbo: {
      if (!ctx.hardcore()) {
        __state = State::GamePaused;
        ctx.pauseGame(true);
        ctx.updateMenu();
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to GamePaused");
#endif
        return true;
      }
      break;
    }
    default: break;
  }

  return false;
}

bool Fsm::quit() {
  switch (__state) {
    case State::CoreLoaded: {
      bool __ok = unloadCore() &&
                  quit();
      return __ok;
    }
    case State::GamePaused: {
      bool __ok = unloadGame() &&
                  unloadCore() &&
                  quit();
      return __ok;
    }
    case State::GameRunning: {
      bool __ok = unloadGame() &&
                  unloadCore() &&
                  quit();
      return __ok;
    }
    case State::GameTurbo: {
      bool __ok = unloadGame() &&
                  unloadCore() &&
                  quit();
      return __ok;
    }
    case State::Start: {
      if (ctx.canQuit()) {
        __state = State::Quit;
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to Quit");
#endif
        return true;
      }
      break;
    }
    default: break;
  }

  return false;
}

bool Fsm::resetGame() {
  switch (__state) {
    case State::GamePaused: {
      bool __ok = resumeGame() &&
                  resetGame();
      return __ok;
    }
    case State::GameRunning: {
      __state = State::GameRunning;
      ctx.resetGame();
      ctx.updateMenu();
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameRunning");
#endif
      return true;
    }
    case State::GameTurbo: {
      bool __ok = normal() &&
                  resetGame();
      return __ok;
    }
    default: break;
  }

  return false;
}

bool Fsm::resumeGame() {
  switch (__state) {
    case State::FrameStep: {
      __state = State::GamePaused;
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GamePaused");
#endif
      return true;
    }
    case State::GamePaused: {
      __state = State::GameRunning;
      ctx.pauseGame(false);
      ctx.updateMenu();
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to GameRunning");
#endif
      return true;
    }
    default: break;
  }

  return false;
}

bool Fsm::step() {
  switch (__state) {
    case State::GamePaused: {
      __state = State::FrameStep;
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to FrameStep");
#endif
      return true;
    }
    default: break;
  }

  return false;
}

bool Fsm::turbo() {
  switch (__state) {
    case State::GamePaused: {
      if (!ctx.hardcore()) {
        __state = State::GameTurbo;
        ctx.updateMenu();
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to GameTurbo");
#endif
        return true;
      }
      break;
    }
    case State::GameRunning: {
      if (!ctx.hardcore()) {
        __state = State::GameTurbo;
        ctx.updateMenu();
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to GameTurbo");
#endif
        return true;
      }
      break;
    }
    default: break;
  }

  return false;
}

bool Fsm::unloadCore() {
  switch (__state) {
    case State::CoreLoaded: {
      if (ctx.canUnloadCore()) {
        __state = State::Start;
        ctx.unloadCore();
        ctx.updateMenu();
#ifdef DEBUG_FSM
        ctx.printf("FSM Switched to Start");
#endif
        return true;
      }
      break;
    }
    default: break;
  }

  return false;
}

bool Fsm::unloadGame() {
  switch (__state) {
    case State::GamePaused: {
      __state = State::CoreLoaded;
      ctx.canUnloadGame();
      ctx.updateMenu();
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to CoreLoaded");
#endif
      return true;
    }
    case State::GameRunning: {
      __state = State::CoreLoaded;
      ctx.unloadGame();
      ctx.updateMenu();
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to CoreLoaded");
#endif
      return true;
    }
    case State::GameTurbo: {
      __state = State::CoreLoaded;
      ctx.unloadGame();
      ctx.updateMenu();
#ifdef DEBUG_FSM
      ctx.printf("FSM Switched to CoreLoaded");
#endif
      return true;
    }
    default: break;
  }

  return false;
}

