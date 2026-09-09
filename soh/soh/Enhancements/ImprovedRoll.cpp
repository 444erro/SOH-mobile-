#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "global.h"
extern "C" void Player_SetupRoll(Player*, PlayState*);
static void RegisterImprovedRoll() {
    COND_VB_SHOULD(VB_PLAYER_ROLL_CHAIN, CVarGetInteger(CVAR_ENHANCEMENT("ImprovedRoll"), 0), {
        Player* player = va_arg(args, Player*); PlayState* play = va_arg(args, PlayState*);
        Input* input = va_arg(args, Input*); s32 floorType = va_arg(args, s32);
        if (player->skelAnime.curFrame >= 15.0f && CHECK_BTN_ALL(input->press.button, BTN_A) && floorType != 7) {
            Player_SetupRoll(player, play); *should = true;
        }
    });
    COND_VB_SHOULD(VB_PLAYER_ROLL_STEER, CVarGetInteger(CVAR_ENHANCEMENT("ImprovedRoll"), 0) &&
                   CVarGetInteger(CVAR_ENHANCEMENT("ImprovedRollSteering"), 0), {
        Player* player = va_arg(args, Player*); PlayState* play = va_arg(args, PlayState*);
        s16 yaw = (s16)va_arg(args, int);
        if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_Z)) Math_ScaledStepToS(&player->actor.shape.rot.y, yaw, 0x200);
        *should = false;
    });
}
static RegisterShipInitFunc initFunc(RegisterImprovedRoll, { CVAR_ENHANCEMENT("ImprovedRoll"), CVAR_ENHANCEMENT("ImprovedRollSteering") });
