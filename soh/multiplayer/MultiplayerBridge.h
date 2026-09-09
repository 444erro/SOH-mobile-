#pragma once

#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

void Multiplayer_Init(void);
void Multiplayer_UpdateLocalPlayer(Player* player, PlayState* play);
void Multiplayer_DrawRemotePlayers(PlayState* play);
void Multiplayer_NotifyVisualEffect(uint8_t itemAction);
void Multiplayer_SetTunicColorOverride(uint8_t enabled, uint8_t r, uint8_t g, uint8_t b);
void Multiplayer_DrawRemotePlayerModel(Player* player, PlayState* play, float x, float y, float z, int16_t rotY,
                                       const Vec3s* jointTable, uint16_t limbCount, uint8_t tunic, uint8_t boots,
                                       uint8_t face, uint8_t shield, uint8_t modelGroup, uint8_t visualFlags,
                                       uint8_t buttonItem0, int8_t itemAction, int8_t heldItemAction,
                                       uint32_t stateFlags1, uint32_t stateFlags2, int8_t invincibilityTimer,
                                       float itemDrawDepth, int16_t getItemDrawId, int8_t actionVar1,
                                       uint8_t movementFlags, const Vec3s* prevTransl, const Vec3s* upperLimbRot,
                                       uint8_t playerAge, uint8_t colorR, uint8_t colorG, uint8_t colorB);

#ifdef __cplusplus
}
#endif
