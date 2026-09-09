#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/OTRGlobals.h"

extern "C" {
#include "z64.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#define CVAR_RESET_BTN_MASK_NAME "gSettings.ResetBtn"

static void OnGameStateMainStartResetHotkey() {
    const uint16_t mask = static_cast<uint16_t>(CVarGetInteger(CVAR_RESET_BTN_MASK_NAME, BTN_CUSTOM_MODIFIER2));
    if (gPlayState != nullptr && mask != 0 && CHECK_BTN_ANY(gPlayState->state.input[0].press.button, mask) &&
        CHECK_BTN_ALL(gPlayState->state.input[0].cur.button, mask)) {
        auto console = std::static_pointer_cast<Ship::ConsoleWindow>(
            Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGuiWindow("Console"));
        if (console != nullptr) {
            console->Dispatch("reset");
        }
    }
}

static void RegisterResetHotkey() {
    COND_HOOK(OnGameStateMainStart, true, OnGameStateMainStartResetHotkey);
}

static RegisterShipInitFunc initFunc(RegisterResetHotkey, { CVAR_RESET_BTN_MASK_NAME });
