#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "variables.h"
extern PlayState* gPlayState;
extern void func_808237B4(PlayState*, Input*);
}
static void OnKaleidoUpdateFaster() {
    ZREG(46) = 2; WREG(6) = 4;
    if (gPlayState->pauseCtx.state == 6 && gPlayState->pauseCtx.unk_1E4 == 1) func_808237B4(gPlayState, gPlayState->state.input);
}
static void RegisterFasterPauseMenu() {
    const bool enabled = CVarGetInteger(CVAR_ENHANCEMENT("FasterPauseMenu"), 0);
    COND_HOOK(GameInteractor::OnKaleidoUpdate, enabled, OnKaleidoUpdateFaster);
    COND_VB_SHOULD(VB_KALEIDO_UNPAUSE_CLOSE, enabled, { ZREG(46) = 1; WREG(6) = 8; });
}
static RegisterShipInitFunc initFunc(RegisterFasterPauseMenu, { CVAR_ENHANCEMENT("FasterPauseMenu") });
