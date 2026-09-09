#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
}
static void OnInitGCDoor(void*) {
    if (GameInteractor_Should(VB_GORONS_CONSIDER_FIRE_TEMPLE_FINISHED, CHECK_QUEST_ITEM(QUEST_MEDALLION_FIRE)) &&
        !Flags_GetInfTable(INFTABLE_GORON_CITY_DOORS_UNLOCKED)) Flags_SetInfTable(INFTABLE_GORON_CITY_DOORS_UNLOCKED);
}
static void RegisterFix() {
    COND_ID_HOOK(OnActorInit, ACTOR_BG_SPOT18_SHUTTER, !IS_RANDO && CVarGetInteger(CVAR_ENHANCEMENT("GCDoorsAfterFireFix"), 0), OnInitGCDoor);
}
static RegisterShipInitFunc initFunc(RegisterFix, { CVAR_ENHANCEMENT("GCDoorsAfterFireFix"), "IS_RANDO" });
