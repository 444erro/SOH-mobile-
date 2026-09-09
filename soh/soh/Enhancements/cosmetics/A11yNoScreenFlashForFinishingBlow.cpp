#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "functions.h"
extern PlayState* gPlayState;
}

static void RegisterNoFinishingBlowFlash() {
    COND_VB_SHOULD(VB_FLASH_SCREEN_FOR_FINISHING_BLOW,
                   CVarGetInteger(CVAR_SETTING("A11yNoScreenFlashForFinishingBlow"), 0), {
        *should = false;
        gPlayState->envCtx.fillScreen = false;
    });
}

static RegisterShipInitFunc initFunc(RegisterNoFinishingBlowFlash,
                                     { CVAR_SETTING("A11yNoScreenFlashForFinishingBlow") });
