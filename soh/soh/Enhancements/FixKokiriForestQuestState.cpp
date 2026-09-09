#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
extern "C" {
#include "src/overlays/actors/ovl_En_Ko/z_en_ko.h"
s32 EnKo_GetForestQuestState(EnKo*);
}
static void RegisterFix() {
    COND_VB_SHOULD(VB_KOKIRI_GET_FOREST_QUEST_STATE2, CVarGetInteger(CVAR_ENHANCEMENT("FixKokiriForestQuestState"), 0), {
        EnKo* kokiri = va_arg(args, EnKo*); kokiri->forestQuestState = EnKo_GetForestQuestState(kokiri); *should = false;
    });
}
static RegisterShipInitFunc initFunc(RegisterFix, { CVAR_ENHANCEMENT("FixKokiriForestQuestState") });
