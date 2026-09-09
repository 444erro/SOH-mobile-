#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
extern "C" { extern PlayState* gPlayState; }
static void DisableSandstorm(int16_t) {
    if (gPlayState != nullptr && gPlayState->sceneNum == SCENE_HAUNTED_WASTELAND)
        gPlayState->envCtx.sandstormState = SANDSTORM_OFF;
}
static void RegisterDisableSandstorm() {
    COND_HOOK(OnTransitionEnd, CVarGetInteger(CVAR_CHEAT("DisableSandstorm"), 0), DisableSandstorm);
}
static RegisterShipInitFunc initFunc(RegisterDisableSandstorm, { CVAR_CHEAT("DisableSandstorm") });
