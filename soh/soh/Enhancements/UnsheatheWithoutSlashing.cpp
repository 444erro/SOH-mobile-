#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

void RegisterUnsheatheWithoutSlashing() {
    COND_VB_SHOULD(VB_USE_HELD_ITEM_AFTER_CHANGE,
                   CVarGetInteger(CVAR_ENHANCEMENT("UnsheatheWithoutSlashing"), 0), {
        Player* player = va_arg(args, Player*);
        ItemID item = static_cast<ItemID>(player->heldItemId);
        if (item == ITEM_SWORD_KOKIRI || item == ITEM_SWORD_MASTER) {
            *should = false;
        }
    });
}
static RegisterShipInitFunc initFunc(RegisterUnsheatheWithoutSlashing,
                                     { CVAR_ENHANCEMENT("UnsheatheWithoutSlashing") });
