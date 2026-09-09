#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include "global.h"

class MultiplayerManager {
public:
    static void Init();
    static void UpdateLocalPlayer(Player* player, PlayState* play);
    static void DrawRemotePlayers(PlayState* play);
    static void NotifyVisualEffect(uint8_t itemAction);
    static void Reconnect();
    static bool IsEnabled();
    static bool IsConnected();
    static bool IsConnecting();
    static size_t GetRemotePlayerCount();
    static std::vector<std::string> GetRemotePlayerNames();
    static int GetLatencyMs();
    static bool SendChatMessage(const std::string& message);
    static void QueueChatMessage(const std::string& message);
    static std::vector<std::string> GetChatMessages();
    static void SetReady(bool ready);
    static bool IsReady();
    static size_t GetReadyPlayerCount();
    static bool SendSignal(uint8_t signal);
};
