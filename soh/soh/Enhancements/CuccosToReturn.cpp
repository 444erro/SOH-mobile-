#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "src/overlays/actors/ovl_En_Niw_Lady/z_en_niw_lady.h"
}
static void RegisterCuccos() {
    COND_VB_SHOULD(VB_SET_CUCCO_COUNT, CVarGetInteger(CVAR_ENHANCEMENT("CuccosToReturn"), 7) != 7, {
        EnNiwLady* lady = va_arg(args, EnNiwLady*); lady->cuccosInPen = 7 - CVarGetInteger(CVAR_ENHANCEMENT("CuccosToReturn"), 7); *should = false;
    });
}
static RegisterShipInitFunc initFunc(RegisterCuccos, { CVAR_ENHANCEMENT("CuccosToReturn") });
