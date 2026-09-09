#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

void RegisterItemUnequip() {
    COND_VB_SHOULD(VB_EQUIP_ITEM_TO_C_BUTTON, CVarGetInteger(CVAR_ENHANCEMENT("ItemUnequip"), 0), {
        PlayState* play = va_arg(args, PlayState*);
        u16 cursorSlot = va_arg(args, int);
        u16 cursorItem = va_arg(args, int);
        Input* input = &play->state.input[0];
        int target = CHECK_BTN_ALL(input->press.button, BTN_CLEFT) ? 1 :
                     CHECK_BTN_ALL(input->press.button, BTN_CDOWN) ? 2 :
                     CHECK_BTN_ALL(input->press.button, BTN_CRIGHT) ? 3 : -1;
        if (target < 0 && CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0)) {
            target = CHECK_BTN_ALL(input->press.button, BTN_DUP) ? 4 :
                     CHECK_BTN_ALL(input->press.button, BTN_DDOWN) ? 5 :
                     CHECK_BTN_ALL(input->press.button, BTN_DLEFT) ? 6 :
                     CHECK_BTN_ALL(input->press.button, BTN_DRIGHT) ? 7 : -1;
        }
        if (target < 0) return;
        u8 equipped = gSaveContext.equips.buttonItems[target];
        u8 equippedSlot = gSaveContext.equips.cButtonSlots[target - 1];
        bool same = equipped == cursorItem &&
                    (!(cursorItem >= ITEM_BOTTLE && cursorItem <= ITEM_POE) || equippedSlot == cursorSlot);
        bool arrow = (cursorItem == ITEM_ARROW_FIRE && equipped == ITEM_BOW_ARROW_FIRE) ||
                     (cursorItem == ITEM_ARROW_ICE && equipped == ITEM_BOW_ARROW_ICE) ||
                     (cursorItem == ITEM_ARROW_LIGHT && equipped == ITEM_BOW_ARROW_LIGHT) ||
                     (cursorItem == ITEM_BOW && (equipped == ITEM_BOW_ARROW_FIRE ||
                                                equipped == ITEM_BOW_ARROW_ICE || equipped == ITEM_BOW_ARROW_LIGHT));
        if (same || arrow) {
            gSaveContext.equips.buttonItems[target] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[target - 1] = SLOT_NONE;
            Interface_LoadItemIcon1(play, target);
            Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            *should = false;
        }
    });
}
static RegisterShipInitFunc initFunc(RegisterItemUnequip, { CVAR_ENHANCEMENT("ItemUnequip") });
