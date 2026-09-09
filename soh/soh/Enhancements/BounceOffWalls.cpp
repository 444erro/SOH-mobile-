#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}
static void RegisterBounceOffWalls() {
    COND_HOOK(OnPlayerUpdate, CVarGetInteger(CVAR_ENHANCEMENT("BounceOffWalls"), 0), []() {
        Player* player = GET_PLAYER(gPlayState);
        if ((player->actor.bgCheckFlags & 0x08) && ABS(player->linearVelocity) > 15.0f) {
            player->yaw = ((player->actor.wallYaw - player->yaw) + player->actor.wallYaw) - 0x8000;
            Player_PlaySfx(&player->actor, NA_SE_PL_BODY_HIT);
        }
    });
}
static RegisterShipInitFunc initFunc(RegisterBounceOffWalls, { CVAR_ENHANCEMENT("BounceOffWalls") });
