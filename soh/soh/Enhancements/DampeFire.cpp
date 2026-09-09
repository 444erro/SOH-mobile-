#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "src/overlays/actors/ovl_En_Po_Relay/z_en_po_relay.h"
}
static void RegisterDampeFire() {
    COND_VB_SHOULD(VB_DAMPE_DROP_FLAME, CVarGetInteger(CVAR_ENHANCEMENT("DampeDropRate"), DAMPE_NORMAL) != DAMPE_NORMAL, {
        double chance; int cooldown = 9;
        switch (CVarGetInteger(CVAR_ENHANCEMENT("DampeDropRate"), DAMPE_NORMAL)) {
            case DAMPE_NONE: *should = false; return; case DAMPE_JALAPENO: chance = .03; break;
            case DAMPE_CHIPOTLE: chance = .1; break; case DAMPE_SCOTCH_BONNET: chance = .2; break;
            case DAMPE_GHOST_PEPPER: chance = .5; cooldown = 4; break; case DAMPE_INFERNO: *should = true; return;
            default: return;
        }
        EnPoRelay* actor = va_arg(args, EnPoRelay*); if (actor->actionTimer > cooldown) actor->actionTimer = cooldown;
        *should = actor->actionTimer == 0 && Rand_ZeroOne() < chance;
    });
}
static RegisterShipInitFunc initFunc(RegisterDampeFire, { CVAR_ENHANCEMENT("DampeDropRate") });
