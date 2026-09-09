#include "MultiplayerBridge.h"
#include "MultiplayerManager.h"
#include "soh/cvar_prefixes.h"
#include "public/bridge/consolevariablebridge.h"
#include <libultraship/libultraship.h>

#if defined(__ANDROID__)
#include <jni.h>
#endif

extern "C" void Multiplayer_Init(void) {
    MultiplayerManager::Init();
}

extern "C" void Multiplayer_UpdateLocalPlayer(Player* player, PlayState* play) {
    MultiplayerManager::UpdateLocalPlayer(player, play);
}

extern "C" void Multiplayer_DrawRemotePlayers(PlayState* play) {
    MultiplayerManager::DrawRemotePlayers(play);
}

extern "C" void Multiplayer_NotifyVisualEffect(uint8_t itemAction) {
    MultiplayerManager::NotifyVisualEffect(itemAction);
}

#if defined(__ANDROID__)
extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_sendMultiplayerChat(JNIEnv* env, jobject,
                                                                                        jstring message) {
    if (message == nullptr) {
        return;
    }
    const char* utfMessage = env->GetStringUTFChars(message, nullptr);
    if (utfMessage != nullptr) {
        MultiplayerManager::QueueChatMessage(utfMessage);
        env->ReleaseStringUTFChars(message, utfMessage);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_setNativeMenuVisible(JNIEnv*, jobject,
                                                                                        jboolean visible) {
    auto context = Ship::Context::GetInstance();
    if (context == nullptr || context->GetWindow() == nullptr || context->GetWindow()->GetGui() == nullptr) {
        return;
    }

    auto menu = context->GetWindow()->GetGui()->GetMenu();
    if (menu != nullptr && menu->IsVisible() != (visible == JNI_TRUE)) {
        menu->ToggleVisibility();
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_toggleNativeMenu(JNIEnv*, jobject) {
    auto context = Ship::Context::GetInstance();
    if (context == nullptr || context->GetWindow() == nullptr || context->GetWindow()->GetGui() == nullptr) {
        return;
    }

    auto menu = context->GetWindow()->GetGui()->GetMenu();
    if (menu != nullptr) {
        menu->ToggleVisibility();
    }
}

extern "C" JNIEXPORT jint JNICALL Java_com_dishii_soh_MainActivity_getNativeMenuLanguage(JNIEnv*, jobject) {
    if (Ship::Context::GetInstance() == nullptr ||
        Ship::Context::GetInstance()->GetConsoleVariables() == nullptr) {
        return -1;
    }
    return CVarGetInteger(CVAR_SETTING("Menu.Language"), 0);
}

#endif
