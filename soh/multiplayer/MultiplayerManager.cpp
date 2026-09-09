#include "MultiplayerManager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "soh/cvar_prefixes.h"
#include "soh/frame_interpolation.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "MultiplayerBridge.h"
#include "NetworkManager.h"

#if defined(__ANDROID__)
#include <jni.h>
#include <SDL2/SDL_system.h>
#endif

static bool sMultiplayerInitialized = false;
static uint32_t sLocalPlayerId = 0;
static std::chrono::steady_clock::time_point sLastConnectionAttempt;
static std::chrono::steady_clock::time_point sLastPositionSend;
static std::chrono::steady_clock::time_point sLastRemoteLog;
static std::chrono::steady_clock::time_point sLastPingSend;
static std::chrono::steady_clock::time_point sLastServerResponse;
static std::chrono::steady_clock::time_point sLastVelocitySample;
static std::chrono::steady_clock::time_point sLastAnchorClientStateSend;
static Vec3f sLastLocalPosition = {};
static bool sVelocitySampleInitialized = false;
static bool sGameplayActive = false;
static bool sConfiguredEnabled = true;
static std::string sConfiguredHost = "127.0.0.1";
static uint16_t sConfiguredPort = 43383;
static std::string sConfiguredPlayerName = "AndroidPlayer";
static std::string sConfiguredRoomCode = "soh-global";
static std::string sConfiguredTeamId = "default";
static bool sHelloSent = false;
static int sLatencyMs = -1;
static std::deque<std::string> sChatMessages;
static bool sLocalReady = false;
static int16_t sLastAnchorScene = -1;
static int32_t sLastAnchorEntrance = -1;
static constexpr const char* ANCHOR_COMPAT_CLIENT_VERSION = "cb71e22";
static constexpr size_t ANCHOR_JOINT_COUNT = 24;
// Bound JSON work so a short network burst cannot consume an entire gameplay frame.
#if defined(__ANDROID__)
// JSON decoding runs on the gameplay thread. Bound it tightly enough that Anchor
// traffic cannot delay the next authentic audio update on mobile devices.
static constexpr size_t MAX_ANCHOR_PACKETS_PER_FRAME = 4;
#else
static constexpr size_t MAX_ANCHOR_PACKETS_PER_FRAME = 16;
#endif
static constexpr size_t MAX_CHAT_BUBBLES_PER_PLAYER = 3;
static constexpr size_t MAX_REMOTE_PLAYERS = 64;
static constexpr auto CHAT_BUBBLE_DURATION = std::chrono::seconds(7);

struct TimedChatMessage {
    std::string text;
    std::chrono::steady_clock::time_point expiresAt;
};

struct RemotePlayerState {
    float x;
    float y;
    float z;
    float renderX;
    float renderY;
    float renderZ;
    int16_t rotY;
    int16_t renderRotY;
    uint16_t scene;
    int8_t room;
    uint8_t tunic;
    uint8_t boots;
    uint8_t face;
    uint8_t shield;
    uint8_t modelGroup;
    uint8_t playerAge;
    uint8_t visualFlags;
    uint8_t buttonItem0;
    int8_t itemAction;
    int8_t heldItemAction;
    uint32_t stateFlags2;
    int8_t invincibilityTimer;
    float itemDrawDepth;
    int16_t getItemDrawId;
    int8_t actionVar1;
    float velocityX;
    float velocityY;
    float velocityZ;
    std::string name;
    std::string signalText;
    std::string nameTagText;
    std::string teamId;
    std::string clientVersion;
    Color_RGB8 color;
    bool colorInitialized;
    uint32_t chatSequence;
    uint32_t privateChatSequence;
    uint32_t visualFxSequence;
    std::deque<TimedChatMessage> chatBubbles;
    std::chrono::steady_clock::time_point signalUntil;
    bool ready;
    bool announced;
    bool online;
    bool isSaveLoaded;
    bool self;
    Actor nameTagActor;
    bool nameTagRegistered;
    bool nameTagDimmed;
    Actor chatTagActor;
    bool chatTagRegistered;
    std::string chatTagText;
    float animFrame;
    bool renderPositionInitialized;
    bool poseInitialized;
    uint16_t limbCount;
    std::array<Vec3s, PLAYER_LIMB_MAX> jointTable;
    uint8_t movementFlags;
    Vec3s prevTransl;
    Vec3s upperLimbRot;
    uint32_t stateFlags1;
    bool interactionPositionLocked;
    Vec3f interactionPosition;
    std::chrono::steady_clock::time_point lastUpdate;
};

static std::unordered_map<uint32_t, RemotePlayerState> sRemotePlayers;
static Player* sLocalPlayer = nullptr;
static constexpr const char* MULTIPLAYER_NAMETAG_TAG = "MultiplayerPlayer";
static Actor sLocalChatActor = {};
static bool sLocalChatTagRegistered = false;
static std::string sLocalChatTagText;
static std::deque<TimedChatMessage> sLocalChatBubbles;
static std::string sLocalAnchorChatMessage;
static uint32_t sLocalAnchorChatSequence = 0;
static std::string sLocalAndroidPrivateChatMessage;
static std::string sLocalAndroidPrivateChatTarget;
static uint32_t sLocalAndroidPrivateChatSequence = 0;
static uint32_t sLocalVisualFxSequence = 0;
static uint8_t sLocalVisualFxItemAction = PLAYER_IA_NONE;
static std::chrono::steady_clock::time_point sLocalAnchorChatClearAt;
static std::chrono::steady_clock::time_point sLocalAndroidPrivateChatClearAt;
static std::mutex sPendingChatMutex;
static std::string sPendingChatMessage;

static RemotePlayerState* FindOrCreateRemotePlayer(uint32_t playerId) {
    const auto existing = sRemotePlayers.find(playerId);
    if (existing != sRemotePlayers.end()) {
        return &existing->second;
    }
    if (sRemotePlayers.size() >= MAX_REMOTE_PLAYERS) {
        SPDLOG_WARN("[Multiplayer] Remote player limit reached");
        return nullptr;
    }
    return &sRemotePlayers.emplace(playerId, RemotePlayerState{}).first->second;
}

static void RemoveRemotePlayer(std::unordered_map<uint32_t, RemotePlayerState>::iterator it) {
    if (it->second.nameTagRegistered) {
        NameTag_RemoveAllForActor(&it->second.nameTagActor);
    }
    if (it->second.chatTagRegistered) {
        NameTag_RemoveAllForActor(&it->second.chatTagActor);
    }
    sRemotePlayers.erase(it);
}

static void RemoveRemotePlayer(uint32_t playerId) {
    const auto it = sRemotePlayers.find(playerId);
    if (it != sRemotePlayers.end()) {
        RemoveRemotePlayer(it);
    }
}

static void ClearRemotePlayers() {
    NameTag_RemoveAllByTag(MULTIPLAYER_NAMETAG_TAG);
    sRemotePlayers.clear();
    sLocalChatTagRegistered = false;
    sLocalChatTagText.clear();
    sLocalChatBubbles.clear();
}

static std::string AppendChatBubbles(std::string text, const std::deque<TimedChatMessage>& bubbles) {
    // Preserve the original message above and append each newer message below it.
    // Every entry keeps its own expiration time.
    for (const auto& message : bubbles) {
        if (!text.empty()) {
            text += "\n";
        }
        text += message.text;
    }
    return text;
}

static int16_t CalculateChatYOffset(const std::string& nameText, float nameScale) {
    // Name tags are world-space billboards, so deriving the offset from the
    // rendered line count keeps the gap stable at every display resolution.
    const int32_t lineCount = 1 + static_cast<int32_t>(std::count(nameText.begin(), nameText.end(), '\n'));
    const float renderedNameHeight = static_cast<float>((lineCount * 16) + 8) * nameScale;
    constexpr int16_t nameYOffset = -2;
    constexpr int16_t minimumGap = 6;
    return static_cast<int16_t>(nameYOffset + std::ceil(renderedNameHeight) + minimumGap);
}

static void RefreshRemoteNameTag(RemotePlayerState& remote) {
    if (remote.name.empty() || sLocalPlayer == nullptr) {
        return;
    }

    // Name tags are globally cleared when a PlayState is destroyed. Keep the multiplayer-side registration
    // state in sync so names are recreated after changing scene or loading a save.
    if (remote.nameTagRegistered && !NameTag_HasForActor(&remote.nameTagActor)) {
        remote.nameTagRegistered = false;
        remote.nameTagText.clear();
    }

    std::string text = remote.name;
    if (!remote.signalText.empty()) {
        text += "\n" + remote.signalText;
    }
    // A remote name is subdued while that player is speaking or while the local player's
    // own chat bubble is visible, keeping the local message visually dominant.
    const bool dimmed = !remote.chatBubbles.empty() || !sLocalChatBubbles.empty();
    const std::string pendingChatText = AppendChatBubbles({}, remote.chatBubbles);
    const bool chatTagCurrent = pendingChatText == remote.chatTagText &&
                                (pendingChatText.empty() ||
                                 (remote.chatTagRegistered && NameTag_HasForActor(&remote.chatTagActor)));
    if (remote.nameTagRegistered && remote.nameTagText == text && remote.nameTagDimmed == dimmed && chatTagCurrent) {
        return;
    }

    if (remote.nameTagRegistered) {
        NameTag_RemoveAllForActor(&remote.nameTagActor);
    } else {
        remote.nameTagActor = {};
        remote.nameTagActor.draw = sLocalPlayer->actor.draw;
        remote.nameTagActor.flags = ACTOR_FLAG_ATTENTION_ENABLED;
        remote.nameTagActor.isDrawn = false;
    }

    const float nameScale = dimmed ? 0.40f : 0.55f;
    NameTagOptions options = {
        .tag = MULTIPLAYER_NAMETAG_TAG,
        .yOffset = -2,
        .textColor = { 120, 220, 255, static_cast<uint8_t>(dimmed ? 82 : 255) },
        .scale = nameScale,
    };
    NameTag_RegisterForActorWithOptions(&remote.nameTagActor, text.c_str(), options);
    remote.nameTagText = text;
    remote.nameTagRegistered = true;
    remote.nameTagDimmed = dimmed;

    const std::string chatText = pendingChatText;
    if (remote.chatTagRegistered) {
        NameTag_RemoveAllForActor(&remote.chatTagActor);
        remote.chatTagRegistered = false;
    }
    remote.chatTagText = chatText;
    if (!chatText.empty()) {
        remote.chatTagActor = {};
        remote.chatTagActor.draw = sLocalPlayer->actor.draw;
        remote.chatTagActor.flags = ACTOR_FLAG_ATTENTION_ENABLED;
        NameTag_RegisterForActorWithOptions(
            &remote.chatTagActor, chatText.c_str(),
            { .tag = MULTIPLAYER_NAMETAG_TAG,
              .yOffset = CalculateChatYOffset(text, nameScale),
              .textColor = { 255, 255, 255, 255 },
              .scale = 0.55f });
        remote.chatTagRegistered = true;
    }
}

static void RefreshLocalChatTag() {
    if (sLocalPlayer == nullptr || sLocalChatBubbles.empty()) {
        if (sLocalChatTagRegistered) {
            NameTag_RemoveAllForActor(&sLocalChatActor);
            sLocalChatTagRegistered = false;
            sLocalChatTagText.clear();
        }
        for (auto& [playerId, remote] : sRemotePlayers) {
            RefreshRemoteNameTag(remote);
        }
        return;
    }

    const std::string text = AppendChatBubbles({}, sLocalChatBubbles);
    if (sLocalChatTagRegistered && text == sLocalChatTagText) {
        return;
    }
    if (sLocalChatTagRegistered) {
        NameTag_RemoveAllForActor(&sLocalChatActor);
    } else {
        sLocalChatActor = {};
        sLocalChatActor.draw = sLocalPlayer->actor.draw;
        sLocalChatActor.flags = ACTOR_FLAG_ATTENTION_ENABLED;
    }
    NameTag_RegisterForActorWithOptions(
        &sLocalChatActor, text.c_str(),
        { .tag = MULTIPLAYER_NAMETAG_TAG,
          .yOffset = 6,
          .textColor = { 255, 255, 255, 255 },
          .scale = 0.55f });
    sLocalChatTagRegistered = true;
    sLocalChatTagText = text;
    for (auto& [playerId, remote] : sRemotePlayers) {
        RefreshRemoteNameTag(remote);
    }
}

static void AddTimedChatBubble(std::deque<TimedChatMessage>& bubbles, const std::string& message,
                               std::chrono::steady_clock::time_point now) {
    bubbles.push_back({ message, now + CHAT_BUBBLE_DURATION });
    while (bubbles.size() > MAX_CHAT_BUBBLES_PER_PLAYER) {
        bubbles.pop_front();
    }
}

static void AddChatMessage(const std::string& message, bool notify, bool compactNotification = false) {
    if (message.empty()) {
        return;
    }
    sChatMessages.push_back(message);
    while (sChatMessages.size() > 20) {
        sChatMessages.pop_front();
    }
    if (notify) {
        if (compactNotification) {
            Notification::Emit({ .message = message,
                                 .remainingTime = 3.0f,
                                 .position = 1,
                                 .fontScale = 0.72f,
                                 .playSound = false });
        } else {
            Notification::Emit({ .message = message });
        }
    }
}

#if defined(__ANDROID__)
static void PushAndroidChatMessage(const std::string& playerName, const std::string& text, Color_RGB8 color) {
    if (playerName.empty() || text.empty()) {
        return;
    }

    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(activityClass, "onMultiplayerChatMessage",
                                       "(Ljava/lang/String;Ljava/lang/String;III)V");
    if (method != nullptr) {
        jstring javaPlayer = env->NewStringUTF(playerName.c_str());
        jstring javaText = env->NewStringUTF(text.c_str());
        env->CallVoidMethod(activity, method, javaPlayer, javaText,
                            static_cast<jint>(color.r),
                            static_cast<jint>(color.g),
                            static_cast<jint>(color.b));
        env->DeleteLocalRef(javaPlayer);
        env->DeleteLocalRef(javaText);
    }
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
}

static void PushAndroidPrivateChatMessage(const std::string& playerName, const std::string& text,
                                          const std::string& targetName, Color_RGB8 color) {
    if (playerName.empty() || text.empty()) {
        return;
    }

    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(activityClass, "onMultiplayerPrivateChatMessage",
                                       "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;III)V");
    if (method != nullptr) {
        jstring javaPlayer = env->NewStringUTF(playerName.c_str());
        jstring javaText = env->NewStringUTF(text.c_str());
        jstring javaTarget = env->NewStringUTF(targetName.c_str());
        env->CallVoidMethod(activity, method, javaPlayer, javaText, javaTarget,
                            static_cast<jint>(color.r),
                            static_cast<jint>(color.g),
                            static_cast<jint>(color.b));
        env->DeleteLocalRef(javaPlayer);
        env->DeleteLocalRef(javaText);
        env->DeleteLocalRef(javaTarget);
    }
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
}
#else
static void PushAndroidChatMessage(const std::string&, const std::string&, Color_RGB8) {
}
static void PushAndroidPrivateChatMessage(const std::string&, const std::string&, const std::string&, Color_RGB8) {
}
#endif

static nlohmann::json JsonVec3f(const Vec3f& value) {
    return {
        { "x", value.x },
        { "y", value.y },
        { "z", value.z },
    };
}

static nlohmann::json JsonVec3s(const Vec3s& value) {
    return {
        { "x", value.x },
        { "y", value.y },
        { "z", value.z },
    };
}

static Vec3f ReadJsonVec3f(const nlohmann::json& value) {
    return {
        value.value("x", 0.0f),
        value.value("y", 0.0f),
        value.value("z", 0.0f),
    };
}

static Vec3s ReadJsonVec3s(const nlohmann::json& value) {
    return {
        static_cast<int16_t>(value.value("x", 0)),
        static_cast<int16_t>(value.value("y", 0)),
        static_cast<int16_t>(value.value("z", 0)),
    };
}

static nlohmann::json MakeAnchorRoomState() {
    if (sConfiguredRoomCode == "soh-global") {
        return {
            { "ownerClientId", 0 },
            { "pvpMode", 0 },
            { "showLocationsMode", 0 },
            { "teleportMode", 0 },
            { "syncItemsAndFlags", 0 },
        };
    }

    return {
        { "ownerClientId", 0 },
        { "pvpMode", 1 },
        { "showLocationsMode", 1 },
        { "teleportMode", 1 },
        { "syncItemsAndFlags", 0 },
    };
}

static nlohmann::json MakeAnchorClientState(PlayState* play) {
    const bool saveLoaded = play != nullptr && sLocalPlayer != nullptr &&
                            gSaveContext.gameMode == GAMEMODE_NORMAL &&
                            gSaveContext.fileNum >= 0 && gSaveContext.fileNum <= 2;

    const Color_RGB8 playerColor =
        CVarGetColor24(CVAR_GENERAL("Multiplayer.PlayerColor.Value"), { 100, 255, 100 });
    nlohmann::json state = {
        { "name", sConfiguredPlayerName },
        { "color", { { "r", playerColor.r }, { "g", playerColor.g }, { "b", playerColor.b } } },
        { "clientVersion", ANCHOR_COMPAT_CLIENT_VERSION },
        { "teamId", sConfiguredTeamId },
        { "online", true },
        { "seed", 0 },
        { "isSaveLoaded", saveLoaded },
        { "isGameComplete", saveLoaded ? gSaveContext.ship.stats.gameComplete : false },
        { "sceneNum", saveLoaded ? play->sceneNum : SCENE_ID_MAX },
        { "entranceIndex", saveLoaded ? gSaveContext.entranceIndex : 0 },
        { "chatMessage", sLocalAnchorChatMessage },
        { "chatSequence", sLocalAnchorChatSequence },
    };

    return state;
}

static bool SendAnchorJson(nlohmann::json payload) {
    payload["clientId"] = sLocalPlayerId;
    return NetworkManager::Instance().SendJson(payload.dump());
}

static bool SendAnchorHandshake(PlayState* play) {
    const nlohmann::json payload = {
        { "type", "HANDSHAKE" },
        { "clientId", sLocalPlayerId },
        { "roomId", sConfiguredRoomCode },
        { "roomState", MakeAnchorRoomState() },
        { "clientState", MakeAnchorClientState(play) },
    };

    const bool sent = NetworkManager::Instance().SendJson(payload.dump());
    if (sent) {
        SPDLOG_DEBUG("[Multiplayer] Anchor handshake sent");
    }
    return sent;
}

static void SendAnchorUpdateClientState(PlayState* play) {
    nlohmann::json payload = {
        { "type", "UPDATE_CLIENT_STATE" },
        { "state", MakeAnchorClientState(play) },
    };
    SendAnchorJson(payload);
}

static void UpdateAnchorClientFromJson(const nlohmann::json& state, std::chrono::steady_clock::time_point now) {
    const uint32_t playerId = state.value("clientId", 0u);
    if (playerId == 0) {
        return;
    }

    const bool self = state.value("self", false) || playerId == sLocalPlayerId;
    if (self) {
        if (sLocalPlayerId != playerId) {
            SPDLOG_INFO("[Multiplayer] Anchor assigned local player id {}", playerId);
            sLocalPlayerId = playerId;
        }
        return;
    }

    const bool reportedOnline = state.value("online", true);
    const uint16_t reportedScene = static_cast<uint16_t>(
        state.value("sceneNum", state.value("scene", static_cast<int>(SCENE_ID_MAX))));
    const bool reportedSaveLoaded = state.value("isSaveLoaded", reportedScene < SCENE_ID_MAX);
    // Public Anchor snapshots may retain many historical/offline clients. Do
    // not let those placeholders consume the bounded remote-player table.
    if (!reportedOnline || !reportedSaveLoaded ||
        (gPlayState != nullptr && reportedScene != gPlayState->sceneNum)) {
        RemoveRemotePlayer(playerId);
        return;
    }

    RemotePlayerState* remotePtr = FindOrCreateRemotePlayer(playerId);
    if (remotePtr == nullptr) {
        return;
    }
    RemotePlayerState& remote = *remotePtr;
    const bool wasOnline = remote.online;
    const std::string newName = state.value("name", "Player " + std::to_string(playerId)).substr(0, 31);
    Color_RGB8 newColor = remote.color;
    if (newColor.r == 0 && newColor.g == 0 && newColor.b == 0) {
        newColor = { 100, 255, 100 };
    }
    if (state.contains("color") && state["color"].is_object()) {
        const auto& color = state["color"];
        newColor = { static_cast<uint8_t>(color.value("r", 100)),
                     static_cast<uint8_t>(color.value("g", 255)),
                     static_cast<uint8_t>(color.value("b", 100)) };
    }
    if ((remote.name != newName || std::memcmp(&remote.color, &newColor, sizeof(Color_RGB8)) != 0) &&
        remote.nameTagRegistered) {
        NameTag_RemoveAllForActor(&remote.nameTagActor);
        remote.nameTagRegistered = false;
    }

    remote.name = newName;
    remote.color = newColor;
    remote.colorInitialized = true;
    remote.teamId = state.value("teamId", std::string{}).substr(0, 63);
    remote.clientVersion = state.value("clientVersion", std::string{}).substr(0, 63);
    remote.online = reportedOnline;
    // Some Anchor PC builds omit isSaveLoaded after the initial room snapshot.
    // Treat a valid scene as loaded instead of leaving both clients waiting for
    // the other side to send the first PLAYER_UPDATE.
    remote.isSaveLoaded = reportedSaveLoaded;
    remote.self = false;
    remote.scene = reportedScene;
    remote.room = sLocalPlayer != nullptr ? sLocalPlayer->actor.room : -1;
    remote.ready = remote.online && remote.isSaveLoaded;
    remote.lastUpdate = now;

    const uint32_t chatSequence = state.value("chatSequence", 0u);
    const std::string chatMessage = state.value("chatMessage", std::string{}).substr(0, 95);
    if (chatSequence > remote.chatSequence && !chatMessage.empty()) {
        remote.chatSequence = chatSequence;
        AddTimedChatBubble(remote.chatBubbles, chatMessage, now);
        AddChatMessage(remote.name + ": " + chatMessage, false);
        PushAndroidChatMessage(remote.name, chatMessage, remote.color);
    }

    RefreshRemoteNameTag(remote);
    if (remote.online && !remote.announced) {
        AddChatMessage(remote.name + " entrou", true, true);
        remote.announced = true;
    } else if (wasOnline && !remote.online) {
        AddChatMessage(remote.name + " saiu", true, true);
        remote.announced = false;
    }
}

static void SpawnAndroidVisualEffect(const RemotePlayerState& remote, uint8_t itemAction) {
    if (gPlayState == nullptr || remote.scene != gPlayState->sceneNum) {
        return;
    }

    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primary = { 255, 255, 255, 220 };
    Color_RGBA8 secondary = { 100, 180, 255, 180 };
    int particleCount = 10;
    float spacing = 12.0f;
    float height = remote.playerAge == LINK_AGE_CHILD ? 32.0f : 46.0f;
    float curve = 0.0f;

    if (itemAction >= PLAYER_IA_BOW && itemAction <= PLAYER_IA_BOW_0E) {
        particleCount = 18;
        spacing = 18.0f;
        primary = { 255, 230, 120, 230 };
        if (itemAction == PLAYER_IA_BOW_FIRE) secondary = { 255, 60, 20, 200 };
        if (itemAction == PLAYER_IA_BOW_ICE) secondary = { 80, 180, 255, 200 };
        if (itemAction == PLAYER_IA_BOW_LIGHT) secondary = { 255, 255, 180, 220 };
    } else if (itemAction == PLAYER_IA_SLINGSHOT) {
        particleCount = 12;
        spacing = 15.0f;
        primary = { 210, 180, 90, 220 };
        secondary = { 100, 70, 30, 180 };
    } else if (itemAction == PLAYER_IA_HOOKSHOT || itemAction == PLAYER_IA_LONGSHOT) {
        particleCount = itemAction == PLAYER_IA_LONGSHOT ? 24 : 16;
        spacing = 14.0f;
        primary = { 210, 210, 220, 230 };
        secondary = { 80, 100, 120, 190 };
    } else if (itemAction == PLAYER_IA_BOOMERANG) {
        particleCount = 18;
        spacing = 10.0f;
        curve = 18.0f;
        primary = { 255, 190, 70, 230 };
        secondary = { 80, 200, 120, 190 };
    } else if (itemAction == PLAYER_IA_BOMB || itemAction == PLAYER_IA_BOMBCHU ||
               itemAction == PLAYER_IA_DEKU_NUT) {
        particleCount = 14;
        spacing = 7.0f;
        height = 12.0f;
        primary = { 255, 150, 40, 230 };
        secondary = { 255, 240, 130, 200 };
    } else if ((itemAction >= PLAYER_IA_MAGIC_SPELL_15 && itemAction <= PLAYER_IA_DINS_FIRE) ||
               itemAction == PLAYER_IA_NAYRUS_LOVE || itemAction == PLAYER_IA_FARORES_WIND) {
        particleCount = 24;
        spacing = 5.0f;
        curve = 28.0f;
        primary = { 180, 120, 255, 230 };
        secondary = { 80, 220, 255, 200 };
    } else if (itemAction >= PLAYER_IA_SWORD_MASTER && itemAction <= PLAYER_IA_HAMMER) {
        particleCount = 14;
        spacing = 4.0f;
        curve = 35.0f;
        primary = { 220, 240, 255, 230 };
        secondary = { 70, 150, 255, 190 };
    } else {
        return;
    }

    const float forwardX = Math_SinS(remote.rotY);
    const float forwardZ = Math_CosS(remote.rotY);
    const float sideX = Math_SinS(remote.rotY + 0x4000);
    const float sideZ = Math_CosS(remote.rotY + 0x4000);
    for (int i = 0; i < particleCount; ++i) {
        const float progress = particleCount > 1 ? static_cast<float>(i) / (particleCount - 1) : 0.0f;
        const float sideOffset = std::sin(progress * 3.14159265f) * curve;
        Vec3f position = {
            remote.x + forwardX * spacing * i + sideX * sideOffset,
            remote.y + height + std::sin(progress * 3.14159265f) * (curve * 0.35f),
            remote.z + forwardZ * spacing * i + sideZ * sideOffset,
        };
        EffectSsKiraKira_SpawnSmall(gPlayState, &position, &velocity, &accel, &primary, &secondary);
    }
}

static void ApplyAnchorPlayerUpdate(const nlohmann::json& payload, std::chrono::steady_clock::time_point now) {
    const uint32_t playerId = payload.value("clientId", 0u);
    if (playerId == 0 || playerId == sLocalPlayerId) {
        return;
    }

    RemotePlayerState* remotePtr = FindOrCreateRemotePlayer(playerId);
    if (remotePtr == nullptr) {
        return;
    }
    RemotePlayerState& remote = *remotePtr;
    if (!remote.colorInitialized) {
        remote.color = { 100, 255, 100 };
        remote.colorInitialized = true;
    }
    remote.online = true;
    remote.isSaveLoaded = true;
    remote.ready = true;
    remote.self = false;
    remote.scene = static_cast<uint16_t>(payload.value("sceneNum", SCENE_ID_MAX));
    remote.room = sLocalPlayer != nullptr ? sLocalPlayer->actor.room : -1;
    remote.playerAge = static_cast<uint8_t>(payload.value("linkAge", static_cast<int>(gSaveContext.linkAge)));

    if (payload.contains("posRot")) {
        const auto& posRot = payload["posRot"];
        if (posRot.contains("pos")) {
            const Vec3f pos = ReadJsonVec3f(posRot["pos"]);
            if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
                return;
            }
            remote.x = pos.x;
            remote.y = pos.y;
            remote.z = pos.z;
        }
        if (posRot.contains("rot")) {
            const Vec3s rot = ReadJsonVec3s(posRot["rot"]);
            remote.rotY = rot.y;
        }
    }

    remote.tunic = static_cast<uint8_t>(payload.value("currentTunic", 0));
    remote.boots = static_cast<uint8_t>(payload.value("currentBoots", 0));
    remote.face = 0;
    remote.shield = static_cast<uint8_t>(payload.value("currentShield", 0));
    remote.modelGroup = static_cast<uint8_t>(payload.value("modelGroup", 0));
    const uint32_t stateFlags1 = payload.value("stateFlags1", 0u);
    remote.stateFlags1 = stateFlags1;
    remote.stateFlags2 = payload.value("stateFlags2", 0u) & ~PLAYER_STATE2_DISABLE_DRAW;
    remote.buttonItem0 = static_cast<uint8_t>(payload.value("buttonItem0", static_cast<int>(ITEM_NONE)));
    remote.itemAction = static_cast<int8_t>(payload.value("itemAction", static_cast<int>(PLAYER_IA_NONE)));
    remote.heldItemAction =
        static_cast<int8_t>(payload.value("heldItemAction", static_cast<int>(PLAYER_IA_NONE)));
    remote.invincibilityTimer = static_cast<int8_t>(payload.value("invincibilityTimer", 0));
    remote.itemDrawDepth = payload.value("unk_85C", 0.0f);
    remote.getItemDrawId = static_cast<int16_t>(payload.value("unk_862", 0));
    remote.actionVar1 = static_cast<int8_t>(payload.value("actionVar1", 0));
    const bool interacting = (stateFlags1 & PLAYER_STATE1_TALKING) != 0;
    if (interacting && !remote.interactionPositionLocked) {
        remote.interactionPosition = remote.renderPositionInitialized
                                         ? Vec3f{ remote.renderX, remote.renderY, remote.renderZ }
                                         : Vec3f{ remote.x, remote.y, remote.z };
        remote.interactionPositionLocked = true;
    } else if (!interacting) {
        remote.interactionPositionLocked = false;
    }
    remote.visualFlags = 0;
    if (stateFlags1 & PLAYER_STATE1_SHIELDING) {
        remote.visualFlags |= 1 << 0;
    }
    if (stateFlags1 & PLAYER_STATE1_HOSTILE_LOCK_ON) {
        remote.visualFlags |= 1 << 1;
    }
    if (stateFlags1 & PLAYER_STATE1_Z_TARGETING) {
        remote.visualFlags |= 1 << 2;
    }
    if (stateFlags1 & PLAYER_STATE1_IN_WATER) {
        remote.visualFlags |= 1 << 3;
    }
    if (stateFlags1 & PLAYER_STATE1_DAMAGED) {
        remote.visualFlags |= 1 << 4;
    }

    remote.velocityX = 0.0f;
    remote.velocityY = 0.0f;
    remote.velocityZ = 0.0f;
    remote.animFrame = 0.0f;
    remote.movementFlags = static_cast<uint8_t>(payload.value("movementFlags", 0));
    if (payload.contains("prevTransl") && payload["prevTransl"].is_object()) {
        remote.prevTransl = ReadJsonVec3s(payload["prevTransl"]);
    }
    if (payload.contains("upperLimbRot") && payload["upperLimbRot"].is_object()) {
        remote.upperLimbRot = ReadJsonVec3s(payload["upperLimbRot"]);
    }

    if (payload.contains("jointTable") && payload["jointTable"].is_array()) {
        const auto& jointTable = payload["jointTable"];
        if (!jointTable.empty() && jointTable.front().is_number()) {
            // Ackbar serializes 24 Vec3s values as one flat array of 72 integers.
            remote.limbCount = static_cast<uint16_t>(
                std::min({ jointTable.size() / 3, ANCHOR_JOINT_COUNT, static_cast<size_t>(PLAYER_LIMB_MAX) }));
            for (size_t i = 0; i < remote.limbCount; ++i) {
                remote.jointTable[i] = {
                    static_cast<int16_t>(jointTable[i * 3].get<int>()),
                    static_cast<int16_t>(jointTable[i * 3 + 1].get<int>()),
                    static_cast<int16_t>(jointTable[i * 3 + 2].get<int>()),
                };
            }
        } else {
            // Keep reading the early Android object format so mixed test builds do not crash.
            remote.limbCount =
                static_cast<uint16_t>(std::min<size_t>(jointTable.size(), PLAYER_LIMB_MAX));
            for (size_t i = 0; i < remote.limbCount; ++i) {
                remote.jointTable[i] = ReadJsonVec3s(jointTable[i]);
            }
        }
        remote.poseInitialized = remote.limbCount > 0;
    }

    if (!remote.poseInitialized && sLocalPlayer != nullptr) {
        remote.limbCount = std::min<uint16_t>(sLocalPlayer->skelAnime.limbCount, PLAYER_LIMB_MAX);
        std::memcpy(remote.jointTable.data(), sLocalPlayer->skelAnime.jointTable,
                    remote.limbCount * sizeof(Vec3s));
        remote.poseInitialized = true;
    }

    const uint32_t chatSequence = payload.value("chatSequence", 0u);
    const std::string chatMessage = payload.value("chatMessage", "");
    if (chatSequence > remote.chatSequence && !chatMessage.empty()) {
        remote.chatSequence = chatSequence;
        AddTimedChatBubble(remote.chatBubbles, chatMessage, now);
        AddChatMessage(remote.name + ": " + chatMessage, false);
        PushAndroidChatMessage(remote.name, chatMessage, remote.color);
    }

    if (payload.contains("androidPrivateChat") && payload["androidPrivateChat"].is_object()) {
        const auto& privateChat = payload["androidPrivateChat"];
        const uint32_t privateSequence = privateChat.value("sequence", 0u);
        const std::string targetName = privateChat.value("target", std::string{}).substr(0, 31);
        const std::string privateMessage = privateChat.value("message", std::string{}).substr(0, 95);
        if (privateSequence > remote.privateChatSequence && !privateMessage.empty() &&
            (targetName.empty() || targetName == sConfiguredPlayerName)) {
            remote.privateChatSequence = privateSequence;
            AddTimedChatBubble(remote.chatBubbles, "@: " + privateMessage, now);
            PushAndroidPrivateChatMessage(remote.name, privateMessage, remote.name, remote.color);
        }
    }

    remote.lastUpdate = now;
    if (payload.contains("androidVisualFx") && payload["androidVisualFx"].is_object()) {
        const auto& visualFx = payload["androidVisualFx"];
        const uint32_t sequence = visualFx.value("sequence", 0u);
        if (sequence > remote.visualFxSequence) {
            remote.visualFxSequence = sequence;
            SpawnAndroidVisualEffect(remote,
                                     static_cast<uint8_t>(visualFx.value("itemAction", PLAYER_IA_NONE)));
        }
    }
    if (!remote.renderPositionInitialized) {
        remote.renderX = remote.x;
        remote.renderY = remote.y;
        remote.renderZ = remote.z;
        remote.renderRotY = remote.rotY;
        remote.renderPositionInitialized = true;
    }
    RefreshRemoteNameTag(remote);
}

static void ProcessAnchorPackets(PlayState* play) {
    std::string message;
    size_t processedPacketCount = 0;
    while (processedPacketCount++ < MAX_ANCHOR_PACKETS_PER_FRAME &&
           NetworkManager::Instance().PollReceivedJson(message)) {
        sLastServerResponse = std::chrono::steady_clock::now();
        try {
            const nlohmann::json payload = nlohmann::json::parse(message);
            const std::string type = payload.value("type", "");
            const auto now = std::chrono::steady_clock::now();

            if (type == "ALL_CLIENT_STATE") {
                if (payload.contains("state") && payload["state"].is_array()) {
                    // This packet is an authoritative snapshot, not a delta.
                    // Drop stale entries before rebuilding the active set.
                    while (!sRemotePlayers.empty()) {
                        RemoveRemotePlayer(sRemotePlayers.begin());
                    }
                    for (const auto& clientEntry : payload["state"]) {
                        nlohmann::json clientState =
                            clientEntry.contains("clientState") ? clientEntry["clientState"] : clientEntry;
                        if (!clientState.contains("clientId") && clientEntry.contains("clientId")) {
                            clientState["clientId"] = clientEntry["clientId"];
                        }
                        UpdateAnchorClientFromJson(clientState, now);
                    }
                }
                SendAnchorUpdateClientState(play);
            } else if (type == "UPDATE_CLIENT_STATE") {
                if (payload.contains("state")) {
                    nlohmann::json state = payload["state"];
                    if (!state.contains("clientId") && payload.contains("clientId")) {
                        state["clientId"] = payload["clientId"];
                    }
                    UpdateAnchorClientFromJson(state, now);
                }
            } else if (type == "PLAYER_UPDATE") {
                ApplyAnchorPlayerUpdate(payload, now);
            } else if (type == "SERVER_MESSAGE") {
                AddChatMessage("Anchor: " + payload.value("message", ""), true);
            }
        } catch (const std::exception& error) {
            // A malformed or incompatible packet must never stop the game loop.
            SPDLOG_WARN("[Multiplayer] Ignored invalid Anchor packet: {}", error.what());
        }
    }
}

static bool HasAnchorPlayerUpdateTarget(PlayState* play) {
    if (play == nullptr) {
        return false;
    }

    for (const auto& [playerId, remote] : sRemotePlayers) {
        if (playerId != sLocalPlayerId && remote.online) {
            return true;
        }
    }
    return false;
}

static void SendAnchorPlayerUpdateTo(uint32_t targetClientId, Player* player, PlayState* play) {
    if (player == nullptr || play == nullptr) {
        return;
    }

    nlohmann::json jointTable = nlohmann::json::array();
    const uint16_t limbCount = std::min<uint16_t>(player->skelAnime.limbCount, PLAYER_LIMB_MAX);
    for (size_t i = 0; i < ANCHOR_JOINT_COUNT; ++i) {
        const Vec3s joint = i < limbCount ? player->skelAnime.jointTable[i] : Vec3s{};
        jointTable.push_back(joint.x);
        jointTable.push_back(joint.y);
        jointTable.push_back(joint.z);
    }

    const uint32_t stateFlags2 =
        static_cast<uint32_t>(player->stateFlags2 & ~PLAYER_STATE2_DISABLE_DRAW);
    const Vec3s shapeRot = {
        player->actor.shape.rot.x,
        player->actor.shape.rot.y,
        player->actor.shape.rot.z,
    };

    nlohmann::json payload = {
        { "type", "PLAYER_UPDATE" },
        { "sceneNum", play->sceneNum },
        { "entranceIndex", gSaveContext.entranceIndex },
        { "linkAge", gSaveContext.linkAge },
        { "posRot", { { "pos", JsonVec3f(player->actor.world.pos) }, { "rot", JsonVec3s(shapeRot) } } },
        { "jointTable", jointTable },
        { "movementFlags", static_cast<uint8_t>(player->skelAnime.moveFlags) },
        { "prevTransl", JsonVec3s(player->skelAnime.prevTransl) },
        { "upperLimbRot", JsonVec3s(player->upperLimbRot) },
        { "currentBoots", player->currentBoots },
        { "currentShield", player->currentShield },
        { "currentTunic", player->currentTunic },
        { "stateFlags1", player->stateFlags1 },
        { "stateFlags2", stateFlags2 },
        { "buttonItem0", gSaveContext.equips.buttonItems[0] },
        { "itemAction", player->itemAction },
        { "heldItemAction", player->heldItemAction },
        { "modelGroup", player->modelGroup },
        { "invincibilityTimer", player->invincibilityTimer },
        { "unk_85C", player->unk_85C },
        { "unk_862", player->unk_862 },
        { "actionVar1", player->av1.actionVar1 },
        { "chatMessage", sLocalAnchorChatMessage },
        { "chatSequence", sLocalAnchorChatSequence },
        { "androidVisualFx",
          { { "version", 1 },
            { "sequence", sLocalVisualFxSequence },
            { "itemAction", sLocalVisualFxItemAction } } },
        { "quiet", true },
        { "targetClientId", targetClientId },
    };

    const auto targetRemote = sRemotePlayers.find(targetClientId);
    if (!sLocalAndroidPrivateChatMessage.empty() && targetRemote != sRemotePlayers.end() &&
        targetRemote->second.name == sLocalAndroidPrivateChatTarget) {
        payload["androidPrivateChat"] = {
            { "version", 1 },
            { "sequence", sLocalAndroidPrivateChatSequence },
            { "target", sLocalAndroidPrivateChatTarget },
            { "message", sLocalAndroidPrivateChatMessage },
        };
    }

    SendAnchorJson(payload);
}

static void SendAnchorPlayerUpdates(Player* player, PlayState* play) {
    for (const auto& [playerId, remote] : sRemotePlayers) {
        // Send to every online peer. Scene/save filtering here caused a protocol
        // deadlock with PC Anchor variants that only publish those fields after
        // receiving their first PLAYER_UPDATE.
        if (playerId != sLocalPlayerId && remote.online) {
            SendAnchorPlayerUpdateTo(playerId, player, play);
        }
    }
}

static std::string GetRemoteDisplayName(uint32_t playerId) {
    const auto remote = sRemotePlayers.find(playerId);
    if (remote != sRemotePlayers.end() && !remote->second.name.empty()) {
        return remote->second.name;
    }
    return "Player " + std::to_string(playerId);
}

static const char* GetSignalText(uint8_t signal) {
    switch (signal) {
        case 0:
            return "Olá!";
        case 1:
            return "Siga-me!";
        case 2:
            return "Espere!";
        case 3:
            return "Ajuda!";
        default:
            return nullptr;
    }
}

static bool RefreshConfiguration() {
    const bool enabled = CVarGetInteger(CVAR_GENERAL("Multiplayer.Enabled"), 1) != 0;
    CVarSetInteger(CVAR_GENERAL("Multiplayer.AnchorMode"), 1);
    const char* configuredHost = CVarGetString(CVAR_GENERAL("Multiplayer.AnchorHost"), "anchor.hm64.org");
    const char* configuredPlayerName =
        CVarGetString(CVAR_GENERAL("Multiplayer.PlayerName"), "AndroidPlayer");
    const char* configuredRoomCode =
        CVarGetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "soh-global");
    const char* configuredTeamId =
        CVarGetString(CVAR_GENERAL("Multiplayer.AnchorTeamId"), "default");
    const int configuredPort = CVarGetInteger(CVAR_GENERAL("Multiplayer.AnchorPort"), 43383);

    std::string host =
        configuredHost != nullptr && configuredHost[0] != '\0'
            ? configuredHost
            : "anchor.hm64.org";
    host = host.substr(0, 253);
    // Touch text input can occasionally persist a partially typed value when
    // the Android keyboard closes. Do not retry an obviously invalid Anchor
    // hostname forever; restore the public Anchor endpoint instead.
    if (host.size() < 3) {
        host = "anchor.hm64.org";
    }
    const std::string playerName = std::string(
        configuredPlayerName != nullptr && configuredPlayerName[0] != '\0' ? configuredPlayerName : "AndroidPlayer"
    ).substr(0, 31);
    const std::string roomCode = std::string(
        configuredRoomCode != nullptr && configuredRoomCode[0] != '\0'
            ? configuredRoomCode
            : "soh-global"
    ).substr(0, 63);
    const std::string teamId = std::string(
        configuredTeamId != nullptr && configuredTeamId[0] != '\0' ? configuredTeamId : "default"
    ).substr(0, 63);
    const uint16_t port = static_cast<uint16_t>(std::clamp(configuredPort, 1, 65535));

    const bool changed = enabled != sConfiguredEnabled || host != sConfiguredHost ||
                         port != sConfiguredPort || playerName != sConfiguredPlayerName ||
                         roomCode != sConfiguredRoomCode || teamId != sConfiguredTeamId;

    sConfiguredEnabled = enabled;
    sConfiguredHost = host;
    sConfiguredPort = port;
    sConfiguredPlayerName = playerName;
    sConfiguredRoomCode = roomCode;
    sConfiguredTeamId = teamId;
    return changed;
}

static int16_t SmoothRotation(int16_t current, int16_t target, float amount) {
    const int16_t difference = static_cast<int16_t>(target - current);
    return static_cast<int16_t>(current + static_cast<int16_t>(difference * amount));
}

static bool IsMultiplayerGameplayValid(Player* player, PlayState* play) {
    if (player == nullptr || play == nullptr || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return false;
    }

    const bool validSaveFile = gSaveContext.fileNum >= 0 && gSaveContext.fileNum <= 2;
    const bool titleOrSpecialCutscene = gSaveContext.fileNum == 0xFEDC || gSaveContext.cutsceneIndex >= 0xFFF0;
    const bool ridingHorse = (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) != 0;

    return validSaveFile && !titleOrSpecialCutscene && !ridingHorse;
}

static void TryConnect() {
    if (!sConfiguredEnabled) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (NetworkManager::Instance().IsConnected() || NetworkManager::Instance().IsConnecting() ||
        now - sLastConnectionAttempt < std::chrono::seconds(3)) {
        return;
    }

    sLastConnectionAttempt = now;
    NetworkManager::Instance().Connect(sConfiguredHost, sConfiguredPort);
}

#if 0 // Removed legacy binary TCP protocol; Anchor JSON is the only supported online mode.
template <typename T>
static T ReadValue(const uint8_t* data) {
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return value;
}

static void ProcessReceivedPackets(PlayState* play) {
    std::vector<uint8_t> packet;
    while (NetworkManager::Instance().PollReceivedPacket(packet)) {
        if (packet.size() < sizeof(PacketHeader)) {
            continue;
        }

        PacketHeader header{};
        std::memcpy(&header, packet.data(), sizeof(header));
        const uint8_t* payload = packet.data() + sizeof(PacketHeader);
        sLastServerResponse = std::chrono::steady_clock::now();

        if (packet.size() < sizeof(PacketHeader) + header.size) {
            continue;
        }

        if (header.type == static_cast<uint16_t>(PacketType::Hello)) {
            if (header.size >= 36) {
                const uint32_t playerId = ReadValue<uint32_t>(payload);
                if (playerId != sLocalPlayerId) {
                    const char* nameStart = reinterpret_cast<const char*>(payload + 4);
                    const size_t nameLength = strnlen(nameStart, 32);
                    RemotePlayerState* remotePtr = FindOrCreateRemotePlayer(playerId);
                    if (remotePtr == nullptr) {
                        continue;
                    }
                    RemotePlayerState& remote = *remotePtr;
                    const std::string newName(nameStart, nameLength);
                    if (remote.name != newName && remote.nameTagRegistered) {
                        NameTag_RemoveAllForActor(&remote.nameTagActor);
                        remote.nameTagRegistered = false;
                    }
                    remote.name = newName;
                    RefreshRemoteNameTag(remote);
                    if (!remote.announced) {
                        AddChatMessage(remote.name + " entrou", true, true);
                        remote.announced = true;
                    }
                    SPDLOG_DEBUG("[Multiplayer] Remote player metadata received");
                }
            }
            continue;
        }

        if (header.type == static_cast<uint16_t>(PacketType::Pong)) {
            if (header.size >= sizeof(uint64_t)) {
                const uint64_t sentAt = ReadValue<uint64_t>(payload);
                const uint64_t now = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count()
                );
                if (now >= sentAt) {
                    sLatencyMs = static_cast<int>(std::min<uint64_t>(now - sentAt, 60000));
                }
            }
            continue;
        }

        if (header.type == static_cast<uint16_t>(PacketType::Chat)) {
            if (header.size >= 100) {
                const uint32_t playerId = ReadValue<uint32_t>(payload);
                if (playerId != sLocalPlayerId) {
                    const char* textStart = reinterpret_cast<const char*>(payload + 4);
                    const size_t textLength = strnlen(textStart, 96);
                    const auto remote = sRemotePlayers.find(playerId);
                    const std::string playerName =
                        remote != sRemotePlayers.end() && !remote->second.name.empty()
                            ? remote->second.name
                            : "Player " + std::to_string(playerId);
                    RemotePlayerState* remotePlayerPtr = FindOrCreateRemotePlayer(playerId);
                    if (remotePlayerPtr == nullptr) {
                        continue;
                    }
                    RemotePlayerState& remotePlayer = *remotePlayerPtr;
                    AddTimedChatBubble(remotePlayer.chatBubbles, std::string(textStart, textLength),
                                       std::chrono::steady_clock::now());
                    RefreshRemoteNameTag(remotePlayer);
                    AddChatMessage(playerName + ": " + std::string(textStart, textLength), true);
                    PushAndroidChatMessage(playerName, std::string(textStart, textLength), remotePlayer.color);
                }
            }
            continue;
        }

        if (header.type == static_cast<uint16_t>(PacketType::PlayerStatus)) {
            if (header.size >= 8) {
                const uint32_t playerId = ReadValue<uint32_t>(payload);
                if (playerId != sLocalPlayerId) {
                    RemotePlayerState* remotePtr = FindOrCreateRemotePlayer(playerId);
                    if (remotePtr == nullptr) {
                        continue;
                    }
                    RemotePlayerState& remote = *remotePtr;
                    remote.ready = ReadValue<uint8_t>(payload + 4) != 0;
                    RefreshRemoteNameTag(remote);
                }
            }
            continue;
        }

        if (header.type == static_cast<uint16_t>(PacketType::Signal)) {
            if (header.size >= 8) {
                const uint32_t playerId = ReadValue<uint32_t>(payload);
                const uint8_t signal = ReadValue<uint8_t>(payload + 4);
                const char* signalText = GetSignalText(signal);
                if (playerId != sLocalPlayerId && signalText != nullptr) {
                    RemotePlayerState* remotePtr = FindOrCreateRemotePlayer(playerId);
                    if (remotePtr == nullptr) {
                        continue;
                    }
                    RemotePlayerState& remote = *remotePtr;
                    remote.signalText = signalText;
                    remote.signalUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
                    RefreshRemoteNameTag(remote);
                    AddChatMessage(GetRemoteDisplayName(playerId) + ": " + signalText, true);
                    PushAndroidChatMessage(GetRemoteDisplayName(playerId), signalText, remote.color);
                }
            }
            continue;
        }

        if (header.type == static_cast<uint16_t>(PacketType::SceneFlag)) {
            if (header.size >= 12 && play != nullptr) {
                const uint32_t playerId = ReadValue<uint32_t>(payload);
                const int16_t scene = ReadValue<int16_t>(payload + 4);
                const int16_t flagType = ReadValue<int16_t>(payload + 6);
                const int16_t flag = ReadValue<int16_t>(payload + 8);
                const bool set = ReadValue<uint8_t>(payload + 10) != 0;

                if (playerId != sLocalPlayerId && scene == play->sceneNum &&
                    ShouldSyncSceneFlag(flagType) && flag >= 0 && flag < 64) {
                    sApplyingRemoteSceneFlag = true;
                    if (flagType == FLAG_SCENE_SWITCH) {
                        if (set) {
                            Flags_SetSwitch(play, flag);
                        } else {
                            Flags_UnsetSwitch(play, flag);
                        }
                    } else if (flagType == FLAG_SCENE_CLEAR) {
                        if (set) {
                            Flags_SetClear(play, flag);
                        } else {
                            Flags_UnsetClear(play, flag);
                        }
                    }
                    sApplyingRemoteSceneFlag = false;
                }
            }
            continue;
        }

        if (header.type == static_cast<uint16_t>(PacketType::PlayerDisconnect)) {
            if (header.size >= sizeof(uint32_t)) {
                const uint32_t playerId = ReadValue<uint32_t>(payload);
                if (playerId != sLocalPlayerId) {
                    const std::string playerName = GetRemoteDisplayName(playerId);
                    RemoveRemotePlayer(playerId);
                    AddChatMessage(playerName + " saiu", true, true);
                    SPDLOG_INFO("[Multiplayer] Remote {} disconnected", playerId);
                }
            }
            continue;
        }

        if (header.type != static_cast<uint16_t>(PacketType::PlayerPosition) || header.size < 42) {
            continue;
        }

        uint32_t playerId = ReadValue<uint32_t>(payload);
        if (playerId == sLocalPlayerId) {
            continue;
        }

        const float receivedX = ReadValue<float>(payload + 4);
        const float receivedY = ReadValue<float>(payload + 8);
        const float receivedZ = ReadValue<float>(payload + 12);
        const float receivedVelocityX = ReadValue<float>(payload + 26);
        const float receivedVelocityY = ReadValue<float>(payload + 30);
        const float receivedVelocityZ = ReadValue<float>(payload + 34);
        const float receivedAnimFrame = ReadValue<float>(payload + 38);
        if (!std::isfinite(receivedX) || !std::isfinite(receivedY) || !std::isfinite(receivedZ) ||
            !std::isfinite(receivedVelocityX) || !std::isfinite(receivedVelocityY) ||
            !std::isfinite(receivedVelocityZ) || !std::isfinite(receivedAnimFrame)) {
            continue;
        }
        RemotePlayerState* remotePtr = FindOrCreateRemotePlayer(playerId);
        if (remotePtr == nullptr) {
            continue;
        }
        RemotePlayerState& remote = *remotePtr;
        RefreshRemoteNameTag(remote);
        remote.x = receivedX;
        remote.y = receivedY;
        remote.z = receivedZ;
        remote.rotY = ReadValue<int16_t>(payload + 16);
        remote.scene = ReadValue<uint16_t>(payload + 18);
        remote.room = ReadValue<int8_t>(payload + 20);
        const uint8_t appearance = ReadValue<uint8_t>(payload + 21);
        remote.tunic = appearance & 0x03;
        remote.boots = (appearance >> 2) & 0x03;
        remote.face = (appearance >> 4) & 0x0F;
        remote.shield = ReadValue<uint8_t>(payload + 22);
        remote.modelGroup = ReadValue<uint8_t>(payload + 23);
        remote.playerAge = ReadValue<uint8_t>(payload + 24);
        remote.visualFlags = ReadValue<uint8_t>(payload + 25);
        static constexpr Color_RGB8 legacyTunicColors[] = {
            { 30, 105, 27 },
            { 100, 20, 0 },
            { 0, 60, 100 },
        };
        remote.color = legacyTunicColors[std::min<size_t>(remote.tunic, 2)];
        remote.colorInitialized = true;
        remote.velocityX = receivedVelocityX;
        remote.velocityY = receivedVelocityY;
        remote.velocityZ = receivedVelocityZ;
        remote.animFrame = receivedAnimFrame;
        remote.lastUpdate = std::chrono::steady_clock::now();

        if (header.size >= 44) {
            const uint16_t poseValueCount = ReadValue<uint16_t>(payload + 42);
            const uint16_t limbCount = poseValueCount / 3;
            const size_t poseBytes = static_cast<size_t>(poseValueCount) * sizeof(int16_t);

            if (poseValueCount % 3 == 0 && limbCount > 0 && limbCount <= PLAYER_LIMB_MAX &&
                header.size >= 44 + poseBytes) {
                std::memcpy(remote.jointTable.data(), payload + 44, poseBytes);
                remote.limbCount = limbCount;
                remote.poseInitialized = true;
            }
        }

        if (!remote.poseInitialized && sLocalPlayer != nullptr) {
            remote.limbCount = std::min<uint16_t>(sLocalPlayer->skelAnime.limbCount, PLAYER_LIMB_MAX);
            std::memcpy(remote.jointTable.data(), sLocalPlayer->skelAnime.jointTable,
                        remote.limbCount * sizeof(Vec3s));
            remote.poseInitialized = true;
        }

        if (!remote.renderPositionInitialized) {
            remote.renderX = remote.x;
            remote.renderY = remote.y;
            remote.renderZ = remote.z;
            remote.renderRotY = remote.rotY;
            remote.renderPositionInitialized = true;
        }

        auto now = remote.lastUpdate;
        if (now - sLastRemoteLog >= std::chrono::seconds(1)) {
            sLastRemoteLog = now;
            SPDLOG_TRACE("[Multiplayer] Remote player state updated");
        }
    }
}
#endif

void MultiplayerManager::Init() {
    if (sMultiplayerInitialized) {
        return;
    }

    sMultiplayerInitialized = true;
    sLocalPlayerId = static_cast<uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    if (sLocalPlayerId == 0) {
        sLocalPlayerId = 1;
    }

    sLastConnectionAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(3);
    sLastPositionSend = std::chrono::steady_clock::now() - std::chrono::milliseconds(100);
    sLastRemoteLog = std::chrono::steady_clock::now();
    sLastPingSend = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    sLastServerResponse = std::chrono::steady_clock::now();
    sLastVelocitySample = std::chrono::steady_clock::now();
    sLastAnchorClientStateSend = std::chrono::steady_clock::now() - std::chrono::seconds(5);

    RefreshConfiguration();
    SPDLOG_INFO("[Multiplayer] Initialized as player {}", sLocalPlayerId);
    TryConnect();
}

void MultiplayerManager::UpdateLocalPlayer(Player* player, PlayState* play) {
    if (!sMultiplayerInitialized || player == nullptr || play == nullptr) {
        return;
    }

    if (RefreshConfiguration()) {
        Reconnect();
    }

    if (!sConfiguredEnabled) {
        if (NetworkManager::Instance().IsConnected()) {
            NetworkManager::Instance().Disconnect();
        }
        sGameplayActive = false;
        sLocalPlayer = nullptr;
        ClearRemotePlayers();
        return;
    }

    if (!IsMultiplayerGameplayValid(player, play)) {
        sGameplayActive = false;
        sLocalPlayer = nullptr;
        ClearRemotePlayers();
        return;
    }

    sGameplayActive = true;
    sLocalPlayer = player;
    TryConnect();
    NetworkManager::Instance().Update();
    if (NetworkManager::Instance().IsConnected()) {
        if (!sHelloSent) {
            sHelloSent = SendAnchorHandshake(play);
            sLastAnchorScene = play->sceneNum;
            sLastAnchorEntrance = gSaveContext.entranceIndex;
            sLastAnchorClientStateSend = std::chrono::steady_clock::now();
            sLastServerResponse = std::chrono::steady_clock::now();
        }
    } else {
        sHelloSent = false;
    }
    ProcessAnchorPackets(play);

    if (!NetworkManager::Instance().IsConnected()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    std::string pendingChat;
    {
        std::lock_guard<std::mutex> lock(sPendingChatMutex);
        pendingChat.swap(sPendingChatMessage);
    }
    if (!pendingChat.empty()) {
        SendChatMessage(pendingChat);
    }
    if (!sLocalAnchorChatMessage.empty() && now >= sLocalAnchorChatClearAt) {
        sLocalAnchorChatMessage.clear();
        SendAnchorUpdateClientState(play);
    }
    if (!sLocalAndroidPrivateChatMessage.empty() && now >= sLocalAndroidPrivateChatClearAt) {
        sLocalAndroidPrivateChatMessage.clear();
        sLocalAndroidPrivateChatTarget.clear();
    }
    const bool sceneChanged = sLastAnchorScene != play->sceneNum ||
                              sLastAnchorEntrance != gSaveContext.entranceIndex;
    if (sceneChanged || now - sLastAnchorClientStateSend >= std::chrono::seconds(5)) {
        SendAnchorUpdateClientState(play);
        sLastAnchorScene = play->sceneNum;
        sLastAnchorEntrance = gSaveContext.entranceIndex;
        sLastAnchorClientStateSend = now;
    }

    if (HasAnchorPlayerUpdateTarget(play) &&
        now - sLastPositionSend >= std::chrono::milliseconds(50)) {
        sLastPositionSend = now;
        SendAnchorPlayerUpdates(player, play);
    }
    return;

#if 0 // Removed legacy binary TCP update path.
    if (now - sLastPingSend >= std::chrono::seconds(2)) {
        const uint64_t timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
        );
        NetworkManager::Instance().Send(Packet::CreatePing(timestamp));
        sLastPingSend = now;
    }

    if (now - sLastServerResponse > std::chrono::seconds(10)) {
        SPDLOG_WARN("[Multiplayer] Server heartbeat timed out");
        Reconnect();
        return;
    }

    if (now - sLastPositionSend < std::chrono::milliseconds(100)) {
        return;
    }
    sLastPositionSend = now;

    float x = player->actor.world.pos.x;
    float y = player->actor.world.pos.y;
    float z = player->actor.world.pos.z;
    int16_t rotY = player->actor.shape.rot.y;
    uint16_t scene = play->sceneNum;
    int8_t room = player->actor.room;
    float animFrame = player->skelAnime.curFrame;
    uint8_t visualFlags = 0;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    const float velocityDeltaSeconds =
        std::chrono::duration<float>(now - sLastVelocitySample).count();
    if (sVelocitySampleInitialized && velocityDeltaSeconds > 0.001f) {
        velocityX = (x - sLastLocalPosition.x) / velocityDeltaSeconds;
        velocityY = (y - sLastLocalPosition.y) / velocityDeltaSeconds;
        velocityZ = (z - sLastLocalPosition.z) / velocityDeltaSeconds;
    }
    sLastLocalPosition = { x, y, z };
    sLastVelocitySample = now;
    sVelocitySampleInitialized = true;
    if (player->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        visualFlags |= 1 << 0;
    }
    if (player->stateFlags1 & PLAYER_STATE1_HOSTILE_LOCK_ON) {
        visualFlags |= 1 << 1;
    }
    if (player->stateFlags1 & PLAYER_STATE1_Z_TARGETING) {
        visualFlags |= 1 << 2;
    }
    if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        visualFlags |= 1 << 3;
    }
    if (player->stateFlags1 & PLAYER_STATE1_DAMAGED) {
        visualFlags |= 1 << 4;
    }

    NetworkManager::Instance().Send(Packet::CreatePosition(
        sLocalPlayerId,
        x,
        y,
        z,
        rotY,
        scene,
        room,
        static_cast<uint8_t>(player->currentTunic),
        static_cast<uint8_t>(player->currentBoots),
        static_cast<uint8_t>(player->actor.shape.face),
        static_cast<uint8_t>(player->currentShield),
        static_cast<uint8_t>(player->modelGroup),
        static_cast<uint8_t>(gSaveContext.linkAge),
        visualFlags,
        velocityX,
        velocityY,
        velocityZ,
        animFrame,
        reinterpret_cast<const int16_t*>(player->skelAnime.jointTable),
        static_cast<uint16_t>(std::min<int>(player->skelAnime.limbCount, PLAYER_LIMB_MAX) * 3)
    ));
#endif
}

void MultiplayerManager::DrawRemotePlayers(PlayState* play) {
    if (!sMultiplayerInitialized || !sGameplayActive || play == nullptr || sLocalPlayer == nullptr ||
        !IsMultiplayerGameplayValid(sLocalPlayer, play)) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    bool localChatChanged = false;
    while (!sLocalChatBubbles.empty() && now >= sLocalChatBubbles.front().expiresAt) {
        sLocalChatBubbles.pop_front();
        localChatChanged = true;
    }
    if (localChatChanged) {
        RefreshLocalChatTag();
    }
    if (sLocalChatTagRegistered) {
        const float headHeight = gSaveContext.linkAge == LINK_AGE_CHILD ? 38.0f : 50.0f;
        sLocalChatActor.world.pos = sLocalPlayer->actor.world.pos;
        sLocalChatActor.focus.pos = sLocalPlayer->actor.world.pos;
        sLocalChatActor.focus.pos.y += headHeight;
        sLocalChatActor.xyzDistToPlayerSq = 0.0f;
        sLocalChatActor.isDrawn = true;
    }

    for (auto it = sRemotePlayers.begin(); it != sRemotePlayers.end();) {
        RemotePlayerState& remote = it->second;
        remote.nameTagActor.isDrawn = false;
        remote.chatTagActor.isDrawn = false;

        if (!remote.signalText.empty() && now >= remote.signalUntil) {
            remote.signalText.clear();
            RefreshRemoteNameTag(remote);
        }
        bool remoteChatChanged = false;
        while (!remote.chatBubbles.empty() && now >= remote.chatBubbles.front().expiresAt) {
            remote.chatBubbles.pop_front();
            remoteChatChanged = true;
        }
        if (remoteChatChanged) {
            RefreshRemoteNameTag(remote);
        }

        if (remote.online && remote.isSaveLoaded &&
            remote.scene == play->sceneNum &&
            remote.renderPositionInitialized) {
            constexpr float smoothing = 0.35f;
            const float predictionSeconds = std::min(
                std::chrono::duration<float>(now - remote.lastUpdate).count(),
                0.25f
            );
            const float targetX = remote.interactionPositionLocked
                                      ? remote.interactionPosition.x
                                      : remote.x + remote.velocityX * predictionSeconds;
            const float targetY = remote.interactionPositionLocked
                                      ? remote.interactionPosition.y
                                      : remote.y + remote.velocityY * predictionSeconds;
            const float targetZ = remote.interactionPositionLocked
                                      ? remote.interactionPosition.z
                                      : remote.z + remote.velocityZ * predictionSeconds;
            const float deltaX = targetX - remote.renderX;
            const float deltaY = targetY - remote.renderY;
            const float deltaZ = targetZ - remote.renderZ;
            const float distanceSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

            if (distanceSquared > 40000.0f) {
                remote.renderX = targetX;
                remote.renderY = targetY;
                remote.renderZ = targetZ;
                remote.renderRotY = remote.rotY;
            } else {
                remote.renderX += deltaX * smoothing;
                remote.renderY += deltaY * smoothing;
                remote.renderZ += deltaZ * smoothing;
                remote.renderRotY = SmoothRotation(remote.renderRotY, remote.rotY, smoothing);
            }

            // Stable per-player interpolation branch. The model is sanitized before drawing,
            // so this branch can be interpolated at display FPS without inheriting local input.
            FrameInterpolation_RecordOpenChild(
                reinterpret_cast<void*>(static_cast<uintptr_t>(it->first)), 0);
            Multiplayer_DrawRemotePlayerModel(
                sLocalPlayer,
                play,
                remote.renderX,
                remote.renderY,
                remote.renderZ,
                remote.renderRotY,
                remote.jointTable.data(),
                remote.limbCount,
                remote.tunic,
                remote.boots,
                remote.face,
                remote.shield,
                remote.modelGroup,
                remote.visualFlags,
                remote.buttonItem0,
                remote.itemAction,
                remote.heldItemAction,
                remote.stateFlags1,
                remote.stateFlags2,
                remote.invincibilityTimer,
                remote.itemDrawDepth,
                remote.getItemDrawId,
                remote.actionVar1,
                (remote.stateFlags1 & PLAYER_STATE1_TALKING) ? 0 : remote.movementFlags,
                &remote.prevTransl,
                &remote.upperLimbRot,
                remote.playerAge,
                remote.color.r,
                remote.color.g,
                remote.color.b
            );
            FrameInterpolation_RecordCloseChild();

            const float headHeight = remote.playerAge == LINK_AGE_CHILD ? 38.0f : 50.0f;
            remote.nameTagActor.world.pos.x = remote.renderX;
            remote.nameTagActor.world.pos.y = remote.renderY;
            remote.nameTagActor.world.pos.z = remote.renderZ;
            remote.nameTagActor.focus.pos.x = remote.renderX;
            remote.nameTagActor.focus.pos.y = remote.renderY + headHeight;
            remote.nameTagActor.focus.pos.z = remote.renderZ;
            // Multiplayer names remain visible at every gameplay distance. Their world position is still used
            // for projection and depth ordering; zero only bypasses the generic name-tag fade/culling threshold.
            remote.nameTagActor.xyzDistToPlayerSq = 0.0f;
            remote.nameTagActor.isDrawn = true;
            if (remote.chatTagRegistered) {
                remote.chatTagActor.world.pos = remote.nameTagActor.world.pos;
                remote.chatTagActor.focus.pos = remote.nameTagActor.focus.pos;
                remote.chatTagActor.xyzDistToPlayerSq = 0.0f;
                remote.chatTagActor.isDrawn = true;
            }
        }

        ++it;
    }
}

void MultiplayerManager::Reconnect() {
    RefreshConfiguration();
    NetworkManager::Instance().Disconnect();
    sHelloSent = false;
    sLatencyMs = -1;
    sLastServerResponse = std::chrono::steady_clock::now();
    sVelocitySampleInitialized = false;
    sLastAnchorScene = -1;
    sLastAnchorEntrance = -1;
    sLastAnchorClientStateSend = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    ClearRemotePlayers();
    sLastConnectionAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(3);
    if (sConfiguredEnabled && sGameplayActive) {
        TryConnect();
    }
}

bool MultiplayerManager::IsEnabled() {
    return CVarGetInteger(CVAR_GENERAL("Multiplayer.Enabled"), 1) != 0;
}

bool MultiplayerManager::IsConnected() {
    return NetworkManager::Instance().IsConnected();
}

bool MultiplayerManager::IsConnecting() {
    return NetworkManager::Instance().IsConnecting();
}

size_t MultiplayerManager::GetRemotePlayerCount() {
    return sRemotePlayers.size();
}

std::vector<std::string> MultiplayerManager::GetRemotePlayerNames() {
    std::vector<std::string> names;
    names.reserve(sRemotePlayers.size());
    for (const auto& [playerId, remote] : sRemotePlayers) {
        if (!remote.name.empty()) {
            names.push_back(remote.name);
        } else {
            names.push_back("Player " + std::to_string(playerId));
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

int MultiplayerManager::GetLatencyMs() {
    return sLatencyMs;
}

bool MultiplayerManager::SendChatMessage(const std::string& message) {
    if (!NetworkManager::Instance().IsConnected() || message.empty()) {
        return false;
    }

    std::string cleanMessage = message.substr(0, 95);
    cleanMessage.erase(std::remove_if(cleanMessage.begin(), cleanMessage.end(),
                                      [](unsigned char c) { return c < 32 && c != ' '; }),
                       cleanMessage.end());
    if (cleanMessage.empty()) {
        return false;
    }

    std::string privateTarget;
    if (cleanMessage[0] == '@') {
        const size_t separator = cleanMessage.find(' ');
        if (separator != std::string::npos && separator > 1 && separator + 1 < cleanMessage.size()) {
            privateTarget = cleanMessage.substr(1, separator - 1).substr(0, 31);
            cleanMessage = cleanMessage.substr(separator + 1).substr(0, 95);
            if (cleanMessage.empty()) {
                return false;
            }
        }
    }

    if (!privateTarget.empty()) {
        bool foundTarget = false;
        for (const auto& [playerId, remote] : sRemotePlayers) {
            if (remote.online && remote.name == privateTarget) {
                foundTarget = true;
                break;
            }
        }
        if (!foundTarget) {
            return false;
        }

        sLocalAndroidPrivateChatTarget = privateTarget;
        sLocalAndroidPrivateChatMessage = cleanMessage;
        ++sLocalAndroidPrivateChatSequence;
        if (sLocalAndroidPrivateChatSequence == 0) {
            sLocalAndroidPrivateChatSequence = 1;
        }
        sLocalAndroidPrivateChatClearAt = std::chrono::steady_clock::now() + CHAT_BUBBLE_DURATION;
        SendAnchorPlayerUpdates(sLocalPlayer, gPlayState);

        AddTimedChatBubble(sLocalChatBubbles, "@: " + cleanMessage, std::chrono::steady_clock::now());
        RefreshLocalChatTag();
        return true;
    }

    sLocalAnchorChatMessage = cleanMessage;
    sLocalAnchorChatSequence++;
    sLocalAnchorChatClearAt = std::chrono::steady_clock::now() + CHAT_BUBBLE_DURATION;
    SendAnchorUpdateClientState(gPlayState);

    AddTimedChatBubble(sLocalChatBubbles, cleanMessage, std::chrono::steady_clock::now());
    RefreshLocalChatTag();
    AddChatMessage(sConfiguredPlayerName + ": " + cleanMessage, false);
    PushAndroidChatMessage(sConfiguredPlayerName, cleanMessage,
                           CVarGetColor24(CVAR_GENERAL("Multiplayer.PlayerColor.Value"), { 100, 255, 100 }));
    return true;
}

void MultiplayerManager::QueueChatMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(sPendingChatMutex);
    sPendingChatMessage = message;
}

void MultiplayerManager::NotifyVisualEffect(uint8_t itemAction) {
    if (itemAction <= PLAYER_IA_NONE || itemAction >= PLAYER_IA_MAX) {
        return;
    }
    sLocalVisualFxItemAction = itemAction;
    ++sLocalVisualFxSequence;
    if (sLocalVisualFxSequence == 0) {
        sLocalVisualFxSequence = 1;
    }
}

std::vector<std::string> MultiplayerManager::GetChatMessages() {
    return { sChatMessages.begin(), sChatMessages.end() };
}

void MultiplayerManager::SetReady(bool ready) {
    if (sLocalReady == ready) {
        return;
    }

    sLocalReady = ready;
}

bool MultiplayerManager::IsReady() {
    return sLocalReady;
}

size_t MultiplayerManager::GetReadyPlayerCount() {
    size_t count = sLocalReady && NetworkManager::Instance().IsConnected() ? 1 : 0;
    for (const auto& [playerId, remote] : sRemotePlayers) {
        if (remote.ready) {
            count++;
        }
    }
    return count;
}

bool MultiplayerManager::SendSignal(uint8_t signal) {
    (void)signal;
    return true;
}
