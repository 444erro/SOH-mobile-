#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "src/overlays/actors/ovl_En_Sa/z_en_sa.h"
extern PlayState* gPlayState;
void EnSa_ChangeAnim(EnSa*, s32);
}
static void RegisterGesture() {
    COND_VB_SHOULD(VB_SARIA_GESTURE, CVarGetInteger(CVAR_ENHANCEMENT("SariaGestureFriendsForever"), 0), {
        const bool inHouse = gPlayState->sceneNum == SCENE_SARIAS_HOUSE;
        *should = *should || inHouse;
        if (inHouse) {
            EnSa* saria = va_arg(args, EnSa*);
            static bool started = false;
            if (saria->unk_20B == 7 && saria->unk_20A == 2) started = true;
            if (started && Animation_OnFrame(&saria->skelAnime, saria->skelAnime.endFrame)) {
                EnSa_ChangeAnim(saria, 4); started = false;
            }
        }
    });
}
static RegisterShipInitFunc initFunc(RegisterGesture, { CVAR_ENHANCEMENT("SariaGestureFriendsForever") });
