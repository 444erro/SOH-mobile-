#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "src/overlays/actors/ovl_En_Door/z_en_door.h"
extern PlayState* gPlayState;
}
static void OnInitDoor(void* actor) {
    EnDoor* door = reinterpret_cast<EnDoor*>(actor);
    if (gPlayState->sceneNum == SCENE_WATER_TEMPLE && ResourceMgr_IsGameMasterQuest() && door->actor.params == 22659)
        door->actor.params = 22660;
}
static void RegisterFix() {
    COND_ID_HOOK(OnActorInit, ACTOR_EN_DOOR, IS_RANDO || CVarGetInteger(CVAR_ENHANCEMENT("MQWaterLockFix"), 0), OnInitDoor);
}
static RegisterShipInitFunc initFunc(RegisterFix, { CVAR_ENHANCEMENT("MQWaterLockFix"), "IS_RANDO" });
