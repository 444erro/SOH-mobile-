#include "SDLGyroMapping.h"
#include "controller/controldevice/controller/mapping/ControllerGyroMapping.h"
#include <spdlog/spdlog.h>
#include "Context.h"

#include "public/bridge/consolevariablebridge.h"
#include "utils/StringHelper.h"

namespace Ship {
SDLGyroMapping::SDLGyroMapping(uint8_t portIndex, float sensitivity, float neutralPitch, float neutralYaw,
                               float neutralRoll)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerGyroMapping(PhysicalDeviceType::SDLGamepad, portIndex, sensitivity), mNeutralPitch(neutralPitch),
      mNeutralYaw(neutralYaw), mNeutralRoll(neutralRoll) {
}

void SDLGyroMapping::Recalibrate() {
    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        if (!SDL_GameControllerHasSensor(gamepad, SDL_SENSOR_GYRO)) {
#ifndef __ANDROID__
            continue;
#endif
        }

        // just use gyro on the first gyro supported device we find
        float gyroData[3];
#ifdef __ANDROID__
        GetAndroidGyroData(gyroData);
#else
        SDL_GameControllerSetSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_TRUE);
        SDL_GameControllerGetSensorData(gamepad, SDL_SENSOR_GYRO, gyroData, 3);
#endif

        mNeutralPitch = gyroData[0];
        mNeutralYaw = gyroData[1];
        mNeutralRoll = gyroData[2];
        return;
    }
    mNeutralPitch = 0;
    mNeutralYaw = 0;
    mNeutralRoll = 0;
}

void SDLGyroMapping::UpdatePad(float& x, float& y) {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        x = 0;
        y = 0;
        return;
    }

    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        if (!SDL_GameControllerHasSensor(gamepad, SDL_SENSOR_GYRO)) {
#ifndef __ANDROID__
            continue
#endif
        }

        // just use gyro on the first gyro supported device we find
        float gyroData[3];
#ifdef __ANDROID__
        GetAndroidGyroData(gyroData);
#else
        SDL_GameControllerSetSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_TRUE);
        SDL_GameControllerGetSensorData(gamepad, SDL_SENSOR_GYRO, gyroData, 3);
#endif

        x = (gyroData[0] - mNeutralPitch) * mSensitivity;
        y = (gyroData[1] - mNeutralYaw) * mSensitivity;
        return;
    }

    // if we didn't find a gyro device zero everything out
    x = 0;
    y = 0;
}

std::string SDLGyroMapping::GetGyroMappingId() {
    return StringHelper::Sprintf("P%d", mPortIndex);
}

void SDLGyroMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".GyroMappings." + GetGyroMappingId();

    CVarSetString(StringHelper::Sprintf("%s.GyroMappingClass", mappingCvarKey.c_str()).c_str(), "SDLGyroMapping");
    CVarSetFloat(StringHelper::Sprintf("%s.Sensitivity", mappingCvarKey.c_str()).c_str(), mSensitivity);
    CVarSetFloat(StringHelper::Sprintf("%s.NeutralPitch", mappingCvarKey.c_str()).c_str(), mNeutralPitch);
    CVarSetFloat(StringHelper::Sprintf("%s.NeutralYaw", mappingCvarKey.c_str()).c_str(), mNeutralYaw);
    CVarSetFloat(StringHelper::Sprintf("%s.NeutralRoll", mappingCvarKey.c_str()).c_str(), mNeutralRoll);

    CVarSave();
}

void SDLGyroMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".GyroMappings." + GetGyroMappingId();

    CVarClear(StringHelper::Sprintf("%s.GyroMappingClass", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.Sensitivity", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.NeutralPitch", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.NeutralYaw", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.NeutralRoll", mappingCvarKey.c_str()).c_str());

    CVarSave();
}

#ifdef __ANDROID__
void SDLGyroMapping::GetAndroidGyroData(float gyroData[3]){
    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        if (!SDL_GameControllerHasSensor(gamepad, SDL_SENSOR_GYRO)) {
            continue;
        }
        SDL_GameControllerSetSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_TRUE);
        SDL_GameControllerGetSensorData(gamepad, SDL_SENSOR_GYRO, gyroData, 3);
        return;
    }

    if(gyroSensor == nullptr){ // populate gyroSensor variable if it hasn't been created yet
        for (int i = 0; i<SDL_NumSensors();i++) {
            if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_GYRO) {
                gyroSensor = SDL_SensorOpen(i);
                break;
            }
        }
    }

    SDL_SensorUpdate();
    SDL_SensorGetData(gyroSensor, gyroData, 3);

    float gyroX = gyroData[0];
    float gyroY = gyroData[1];
    switch(SDL_GetDisplayOrientation(0)){
        case(SDL_ORIENTATION_PORTRAIT):
            // nothing to do
            break;
        case(SDL_ORIENTATION_PORTRAIT_FLIPPED):
            gyroData[0] = -gyroX;
            gyroData[1] = -gyroY;
            break;
        case(SDL_ORIENTATION_LANDSCAPE):
            gyroData[0] = -gyroY;
            gyroData[1] = gyroX;
            break;
        case(SDL_ORIENTATION_LANDSCAPE_FLIPPED):
            gyroData[0] = gyroY;
            gyroData[1] = -gyroX;
            break;
        case(SDL_ORIENTATION_UNKNOWN):
            // nothing to do
            break;
    }
}
#endif

std::string SDLGyroMapping::GetPhysicalDeviceName() {
    return "SDL Gamepad";
}
} // namespace Ship
