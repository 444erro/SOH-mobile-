#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "functions.h"
#include "variables.h"
}
static void RegisterDivingTimer() {
    COND_VB_SHOULD(VB_SET_DIVING_GAME_TIME_LIMIT, CVarGetInteger(CVAR_ENHANCEMENT("DivingGame.TimeLimit"), 50) != 50, {
        func_80088B34(BREG(2) + CVarGetInteger(CVAR_ENHANCEMENT("DivingGame.TimeLimit"), 50)); *should = false;
    });
}
static RegisterShipInitFunc initFunc(RegisterDivingTimer, { CVAR_ENHANCEMENT("DivingGame.TimeLimit") });
