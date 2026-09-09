#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "global.h"
static void RegisterFasterBottleEmpty() {
    COND_VB_SHOULD(VB_EMPTYING_BOTTLE, CVarGetInteger(CVAR_ENHANCEMENT("FasterBottleEmpty"), 0), {
        Player* player = va_arg(args, Player*);
        player->skelAnime.playSpeed = player->skelAnime.curFrame <= 60.0f ? 3.0f : 1.0f;
    });
}
static RegisterShipInitFunc initFunc(RegisterFasterBottleEmpty, { CVAR_ENHANCEMENT("FasterBottleEmpty") });
