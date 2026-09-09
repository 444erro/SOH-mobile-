#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/OTRGlobals.h"

extern "C" {
#include "z64.h"
extern PlayState* gPlayState;
}

#define CVAR_REWORKED_TARGETING CVAR_ENHANCEMENT("ReworkedTargeting.Enabled")
static bool sTriggeredByButtonCombo = false;

static void RegisterReworkedTargeting() {
    COND_VB_SHOULD(VB_TOGGLE_Z_TARGET_SWITCH_TARGETS, CVarGetInteger(CVAR_REWORKED_TARGETING, 0), {
        Player* player = GET_PLAYER(gPlayState);
        if (player->focusActor != nullptr && !sTriggeredByButtonCombo) {
            *should = false;
        }
        sTriggeredByButtonCombo = false;
    });
    COND_VB_SHOULD(VB_TOGGLE_Z_TARGET_SWITCH_DIRECTION, CVarGetInteger(CVAR_REWORKED_TARGETING, 0), {
        if (*should) return;
        Player* player = GET_PLAYER(gPlayState);
        if (player->focusActor != nullptr) {
            Input* input = &gPlayState->state.input[0];
            const int32_t mask = CVarGetInteger(CVAR_ENHANCEMENT("ReworkedTargeting.Btn"), 0);
            if (mask != 0 && CHECK_BTN_ANY(input->press.button, mask)) {
                sTriggeredByButtonCombo = gPlayState->actorCtx.targetCtx.unk_94 != nullptr;
                *should = sTriggeredByButtonCombo;
            }
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterReworkedTargeting, { CVAR_REWORKED_TARGETING });
