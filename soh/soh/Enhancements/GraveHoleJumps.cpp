#include "soh/ShipInit.hpp"
#include "functions.h"
#include "soh/resource/type/Scene.h"
#include "soh/resource/type/scenecommand/SceneCommand.h"
#include "soh/resource/type/scenecommand/SetCollisionHeader.h"
#include "Context.h"
#include "public/bridge/consolevariablebridge.h"
#include <array>
#include <cstring>

#define CVAR_GRAVE_HOLES CVAR_ENHANCEMENT("GraveHoles")
#define GRAVEYARD_SCENE "scenes/shared/spot02_scene/spot02_scene"
#define CUSTOM_SURFACE_TYPE 32

static const std::array<std::pair<std::pair<u16, u16>, std::pair<u16, u16>>, 6> sPatches = { {
    { { 487, 509 }, { 20, CUSTOM_SURFACE_TYPE } }, { { 651, 658 }, { 20, CUSTOM_SURFACE_TYPE } },
    { { 613, 620 }, { 0, 15 } }, { { 623, 630 }, { 0, 15 } },
    { { 633, 640 }, { 0, 15 } }, { { 643, 650 }, { 0, 15 } },
} };

static CollisionHeader* GetGraveyardCollision() {
    SOH::Scene* scene = static_cast<SOH::Scene*>(Ship::Context::GetInstance()->GetResourceManager()
                                                    ->LoadResource(GRAVEYARD_SCENE).get());
    SOH::SetCollisionHeader* collisionCommand = nullptr;
    for (const auto& command : scene->commands) {
        if (command->cmdId == SOH::SceneCommandID::SetCollisionHeader) {
            collisionCommand = static_cast<SOH::SetCollisionHeader*>(command.get());
            break;
        }
    }
    if (collisionCommand == nullptr) return nullptr;
    CollisionHeader* header = static_cast<CollisionHeader*>(collisionCommand->GetRawPointer());
    static SurfaceType surfaces[33];
    memcpy(surfaces, header->surfaceTypeList, sizeof(SurfaceType) * collisionCommand->collisionHeader->surfaceTypesCount);
    surfaces[CUSTOM_SURFACE_TYPE].data[0] = 0x24000004;
    surfaces[CUSTOM_SURFACE_TYPE].data[1] = 0xFC8;
    header->surfaceTypeList = surfaces;
    return header;
}

static void ApplyGraveHoleGeometry() {
    static CollisionHeader* header = GetGraveyardCollision();
    if (header == nullptr) return;
    for (const auto& patch : sPatches) {
        for (int i = patch.first.first; i <= patch.first.second; i++)
            header->polyList[i].type = CVarGetInteger(CVAR_GRAVE_HOLES, 0) ? patch.second.first : patch.second.second;
    }
}

static RegisterShipInitFunc initFunc(ApplyGraveHoleGeometry, { CVAR_GRAVE_HOLES });
