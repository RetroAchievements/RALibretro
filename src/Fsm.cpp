// Generated with FSM compiler, https://github.com/leiradel/ddlt

#include "Fsm.h"
#include "Application.h"

#ifdef DEBUG_FSM
const char* Fsm::stateName(State state) const {
  switch (state) {
    case State::CoreLoaded: return "CoreLoaded";
    case State::FrameStep: return "FrameStep";
    case State::GamePaused: return "GamePaused";
    case State::GamePausedNoOvl: return "GamePausedNoOvl";
    case State::GameRunning: return "GameRunning";
    case State::Quit: return "Quit";
    case State::Start: return "Start";
    default: break;
  }

  return NULL;
}
#endif

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

bool Fsm::loadCore(const_string core) {
  switch (__state) {
    case State::CoreLoaded: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      bool __ok = unloadCore() &&
                  loadCore(core);

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif
      }

      return __ok;
    }
    break;

    case State::GamePaused: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  unloadCore() &&
                  loadCore(core);

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif
      }

      return __ok;
    }
    break;

    case State::GamePausedNoOvl: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  unloadCore() &&
                  loadCore(core);

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif
      }

      return __ok;
    }
    break;

    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  unloadCore() &&
                  loadCore(core);

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif
      }

      return __ok;
    }
    break;

    case State::Start: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }



      if (!ctx.loadCore(core)) {

        return false;

      }

    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
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
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }


      if (!ctx.loadGame(path)) {
        return false;
      }
    
      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      return true;
    }
    break;

    case State::GamePaused: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  loadGame(path);

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      }

      return __ok;
    }
    break;

    case State::GamePausedNoOvl: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  loadGame(path);

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      }

      return __ok;
    }
    break;

    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  loadGame(path);

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      }

      return __ok;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::pauseGame() {
  switch (__state) {
    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GamePaused));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GamePaused));
#endif

        return false;
      }

      __state = State::GamePaused;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::GamePaused));
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::pauseGameNoOvl() {
  switch (__state) {
    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GamePausedNoOvl));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GamePausedNoOvl));
#endif

        return false;
      }



      if (ctx.hardcore()) {

        return false;

      }

    
      __state = State::GamePausedNoOvl;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::GamePausedNoOvl));
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
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      bool __ok = unloadCore() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif
      }

      return __ok;
    }
    break;

    case State::GamePaused: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif
      }

      return __ok;
    }
    break;

    case State::GamePausedNoOvl: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif
      }

      return __ok;
    }
    break;

    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      bool __ok = unloadGame() &&
                  quit();

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif
      }

      return __ok;
    }
    break;

    case State::Start: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
#endif

        return false;
      }

      __state = State::Quit;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::Quit));
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
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      bool __ok = resumeGame() &&
                  resetGame();

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      }

      return __ok;
    }
    break;

    case State::GamePausedNoOvl: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      bool __ok = resumeGame() &&
                  resetGame();

      if (__ok) {
        after(__state);
        after();

      }
      else {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed to switch to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      }

      return __ok;
    }
    break;

    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }


      ctx.resetGame();
    
      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

bool Fsm::resumeGame() {
  switch (__state) {
    case State::FrameStep: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GamePaused));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GamePaused));
#endif

        return false;
      }

      __state = State::GamePaused;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::GamePaused));
#endif
      return true;
    }
    break;

    case State::GamePaused: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif
      return true;
    }
    break;

    case State::GamePausedNoOvl: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
#endif

        return false;
      }

      __state = State::GameRunning;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::GameRunning));
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
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif

        return false;
      }



      if (ctx.hardcore()) {

        return false;

      }

      __state = State::FrameStep;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif
      return true;
    }
    break;

    case State::GamePausedNoOvl: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif

        return false;
      }



      if (ctx.hardcore()) {

        return false;

      }

      __state = State::FrameStep;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif
      return true;
    }
    break;

    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
#endif

        return false;
      }



      if (ctx.hardcore()) {

        return false;

      }

    
      __state = State::FrameStep;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::FrameStep));
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
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Start));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::Start));
#endif

        return false;
      }


      ctx.unloadCore();
    
      __state = State::Start;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::Start));
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
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }


      if (!ctx.unloadGame()) {
        return false;
      }
    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif
      return true;
    }
    break;

    case State::GamePausedNoOvl: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }


      if (!ctx.unloadGame()) {
        return false;
      }
    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif
      return true;
    }
    break;

    case State::GameRunning: {
      if (!before()) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed global precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }

      if (!before(__state)) {
#ifdef DEBUG_FSM
        ctx.printf("FSM %s:%u Failed state precondition while switching to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif

        return false;
      }



      if (!ctx.unloadGame()) {

        return false;

      }

    
      __state = State::CoreLoaded;
      after(__state);
      after();

#ifdef DEBUG_FSM
      ctx.printf("FSM %s:%u Switched to %s", __FUNCTION__, __LINE__, stateName(State::CoreLoaded));
#endif
      return true;
    }
    break;

    default: break;
  }

  return false;
}

