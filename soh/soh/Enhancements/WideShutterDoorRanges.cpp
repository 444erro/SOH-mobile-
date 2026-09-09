#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "src/overlays/actors/ovl_Door_Shutter/z_door_shutter.h"
}
static void RegisterWideRange() {
    COND_VB_SHOULD(VB_BE_NEAR_DOOR_SHUTTER, CVarGetInteger(CVAR_ENHANCEMENT("WideShutterDoorRange"), 0), {
        DoorShutter* door = va_arg(args, DoorShutter*); Vec3f rel = *va_arg(args, Vec3f*);
        if (door->unk_16C == 3 || door->unk_16C == 4 || door->unk_16C == 5 || door->unk_16C == 7)
            *should = 70.0f < fabsf(rel.x) || 15.0f < fabsf(rel.y);
    });
}
static RegisterShipInitFunc initFunc(RegisterWideRange, { CVAR_ENHANCEMENT("WideShutterDoorRange") });
