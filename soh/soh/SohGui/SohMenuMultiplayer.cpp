#include "SohMenu.h"
#include "SohGui.hpp"

#include "multiplayer/MultiplayerManager.h"
#ifdef ENABLE_REMOTE_CONTROL
#include "soh/Network/CrowdControl/CrowdControl.h"
#endif
#include <algorithm>
#include <string>

#if 0
namespace SohGui {

using namespace UIWidgets;
static std::string sMultiplayerChatInput;

#if defined(__ANDROID__)
static constexpr uint32_t MULTIPLAYER_MENU_COLUMNS = 1;
#else
static constexpr uint32_t MULTIPLAYER_MENU_COLUMNS = 2;
#endif

static bool IsAnchorModeSelected() {
    return CVarGetInteger(CVAR_GENERAL("Multiplayer.AnchorMode"), 1) != 0;
}

void SohMenu::AddMenuMultiplayer() {
    // Anchor is the supported online mode exposed by the Android interface.
    CVarSetInteger(CVAR_GENERAL("Multiplayer.AnchorMode"), 1);
    AddMenuEntry("Multiplayer", CVAR_SETTING("Menu.MultiplayerSidebarSection"));

    WidgetPath path = { "Multiplayer", "Connection", SECTION_COLUMN_1 };
    AddSidebarEntry("Multiplayer", path.sidebarName, MULTIPLAYER_MENU_COLUMNS);

    AddWidget(path,
              "Connects this game to multiplayer. Anchor compatibility talks to the same "
              "online service used by the PC Ackbar build. The legacy TCP mode is kept for "
              "local Android-only tests.",
              WIDGET_TEXT);

    AddWidget(path, "Enable Multiplayer", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("Multiplayer.Enabled"))
        .RaceDisable(false)
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Connect automatically while a normal save file is being played."));

    AddWidget(path, "Anchor compatibility mode", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("Multiplayer.AnchorMode"))
        .RaceDisable(false)
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Use the Anchor JSON protocol used by the PC Ackbar online build."));

    AddWidget(path, "Anchor online", WIDGET_SEPARATOR_TEXT)
        .PreFunc([](WidgetInfo& info) { info.isHidden = !IsAnchorModeSelected(); });

    AddWidget(path, "Anchor server and port", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float portWidth = std::clamp(availableWidth * 0.22f, ImGui::GetFontSize() * 5.0f,
                                           ImGui::GetFontSize() * 7.0f);
        const float separatorWidth = ImGui::CalcTextSize(":").x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        const float hostWidth = std::max(ImGui::GetFontSize() * 8.0f,
                                         availableWidth - portWidth - separatorWidth);
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##MultiplayerAnchorHost", CVAR_GENERAL("Multiplayer.AnchorHost"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("anchor.hm64.org")
                            .DefaultValue("anchor.hm64.org")
                            .Size(ImVec2(hostWidth, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        CVarInputInt("##MultiplayerAnchorPort", CVAR_GENERAL("Multiplayer.AnchorPort"),
                     InputOptions()
                         .Color(THEME_COLOR)
                         .PlaceholderText("43383")
                         .DefaultValue("43383")
                         .Size(ImVec2(portWidth, 0))
                         .LabelPosition(LabelPositions::None));
    }).PreFunc([](WidgetInfo& info) { info.isHidden = !IsAnchorModeSelected(); });

    AddWidget(path, "Anchor room", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##MultiplayerAnchorRoom", CVAR_GENERAL("Multiplayer.AnchorRoomId"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("soh-global")
                            .DefaultValue("soh-global")
                            .Size(ImVec2(ImGui::GetContentRegionAvail().x, 0))
                            .LabelPosition(LabelPositions::None));
    }).PreFunc([](WidgetInfo& info) { info.isHidden = !IsAnchorModeSelected(); });

    AddWidget(path, "Anchor team", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##MultiplayerAnchorTeam", CVAR_GENERAL("Multiplayer.AnchorTeamId"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("default")
                            .DefaultValue("default")
                            .Size(ImVec2(ImGui::GetContentRegionAvail().x, 0))
                            .LabelPosition(LabelPositions::None));
    }).PreFunc([](WidgetInfo& info) { info.isHidden = !IsAnchorModeSelected(); });

    AddWidget(path, "Restore Anchor defaults", WIDGET_BUTTON)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) { info.isHidden = !IsAnchorModeSelected(); })
        .Callback([](WidgetInfo& info) {
            CVarSetString(CVAR_GENERAL("Multiplayer.AnchorHost"), "anchor.hm64.org");
            CVarSetInteger(CVAR_GENERAL("Multiplayer.AnchorPort"), 43383);
            CVarSetString(CVAR_GENERAL("Multiplayer.AnchorTeamId"), "default");
            const char* room = CVarGetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "");
            if (room == nullptr || room[0] == '\0') {
                CVarSetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "soh-global");
            }
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            MultiplayerManager::Reconnect();
        })
        .Options(ButtonOptions().Tooltip("Restores anchor.hm64.org:43383 and reconnects."));

    AddWidget(path, "Legacy local TCP", WIDGET_SEPARATOR_TEXT)
        .PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path, "Server address and port", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float portWidth = std::clamp(availableWidth * 0.22f, ImGui::GetFontSize() * 5.0f,
                                           ImGui::GetFontSize() * 7.0f);
        const float separatorWidth = ImGui::CalcTextSize(":").x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        const float hostWidth = std::max(ImGui::GetFontSize() * 8.0f,
                                         availableWidth - portWidth - separatorWidth);
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##MultiplayerHost", CVAR_GENERAL("Multiplayer.Host"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("127.0.0.1")
                            .DefaultValue("127.0.0.1")
                            .Size(ImVec2(hostWidth, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        CVarInputInt("##MultiplayerPort", CVAR_GENERAL("Multiplayer.Port"),
                     InputOptions()
                         .Color(THEME_COLOR)
                         .PlaceholderText("43383")
                         .DefaultValue("43383")
                         .Size(ImVec2(portWidth, 0))
                         .LabelPosition(LabelPositions::None));
    });

    AddWidget(path, "Player name", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##MultiplayerPlayerName", CVAR_GENERAL("Multiplayer.PlayerName"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("AndroidPlayer")
                            .DefaultValue("AndroidPlayer")
                            .Size(ImVec2(ImGui::GetContentRegionAvail().x, 0))
                            .LabelPosition(LabelPositions::None));
    }).PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path, "Room code", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##MultiplayerRoomCode", CVAR_GENERAL("Multiplayer.RoomCode"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("default")
                            .DefaultValue("default")
                            .Size(ImVec2(ImGui::GetContentRegionAvail().x, 0))
                            .LabelPosition(LabelPositions::None));
    }).PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path,
              "For PC compatibility, leave Anchor mode enabled and use the same Anchor room as the PC. "
              "The legacy address/room fields are only for our local TCP test server.",
              WIDGET_TEXT)
        .PreFunc([](WidgetInfo& info) {
            info.name = IsAnchorModeSelected()
                            ? "Use the same Anchor room as the PC. The room is created automatically when the first "
                              "player connects."
                            : "Legacy TCP is intended for local Android tests with the companion TCP server.";
        });

    AddWidget(path, "Shared scene switches", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("Multiplayer.SyncSceneSwitches"))
        .RaceDisable(false)
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Synchronizes room switches and mechanisms in real time. "
                              "Chests, collectibles and permanent progression remain local."))
        .PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path, "Shared room clear state", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("Multiplayer.SyncSceneClear"))
        .RaceDisable(false)
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Shares whether a combat room has been cleared. "
                              "This can open doors tied to defeating all enemies."))
        .PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path, "Reconnect now", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            MultiplayerManager::Reconnect();
        })
        .Options(ButtonOptions().Tooltip("Disconnects and immediately applies the current server settings."));

    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Status", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Multiplayer disabled", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        if (!MultiplayerManager::IsEnabled()) {
            info.name = "Multiplayer disabled";
        } else if (MultiplayerManager::IsConnected()) {
            info.name = "Connected to server";
        } else if (MultiplayerManager::IsConnecting()) {
            info.name = "Connecting to server";
        } else {
            info.name = "Waiting for connection";
        }
    });

    AddWidget(path, "Remote players: 0", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.name = "Remote players: " + std::to_string(MultiplayerManager::GetRemotePlayerCount());
    });

    AddWidget(path, "Latency: --", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        const int latency = MultiplayerManager::GetLatencyMs();
        info.name = latency >= 0 ? "Latency: " + std::to_string(latency) + " ms" : "Latency: --";
    });

    AddWidget(path, "Anchor room: soh-global", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !IsAnchorModeSelected();
        const char* room = CVarGetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "soh-global");
        info.name = "Anchor room: " + std::string(room != nullptr && room[0] != '\0' ? room : "soh-global");
    });

    AddWidget(path, "Lobby readiness", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::SeparatorText(info.name.c_str());
        bool ready = MultiplayerManager::IsReady();
        ImGui::BeginDisabled(!MultiplayerManager::IsConnected());
        if (ImGui::Checkbox("I am ready", &ready)) {
            MultiplayerManager::SetReady(ready);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled(
            "(%zu/%zu ready)",
            MultiplayerManager::GetReadyPlayerCount(),
            MultiplayerManager::GetRemotePlayerCount() + (MultiplayerManager::IsConnected() ? 1 : 0)
        );
    }).PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path, "Player list", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::SeparatorText(info.name.c_str());
        const auto playerNames = MultiplayerManager::GetRemotePlayerNames();
        if (playerNames.empty()) {
            ImGui::TextDisabled("No remote players");
            return;
        }
        for (const auto& playerName : playerNames) {
            ImGui::BulletText("%s", playerName.c_str());
        }
    });

    AddWidget(path, "Room chat", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::SeparatorText(info.name.c_str());
        const auto messages = MultiplayerManager::GetChatMessages();
        ImGui::BeginChild("##MultiplayerChatHistory", ImVec2(0, ImGui::GetFontSize() * 8), true);
        for (const auto& message : messages) {
            ImGui::TextWrapped("%s", message.c_str());
        }
        ImGui::EndChild();

        InputString("##MultiplayerChatInput", &sMultiplayerChatInput,
                    InputOptions()
                        .Color(THEME_COLOR)
                        .PlaceholderText("Type a message")
                        .Size(ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 5, 0))
                        .LabelPosition(LabelPositions::None));
        ImGui::SameLine();
        ImGui::BeginDisabled(!MultiplayerManager::IsConnected() || sMultiplayerChatInput.empty());
        if (ImGui::Button("Send")) {
            if (MultiplayerManager::SendChatMessage(sMultiplayerChatInput)) {
                sMultiplayerChatInput.clear();
            }
        }
        ImGui::EndDisabled();
    }).PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path, "Quick signals", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::SeparatorText(info.name.c_str());
        ImGui::BeginDisabled(!MultiplayerManager::IsConnected());
        if (ImGui::Button("Hello")) {
            MultiplayerManager::SendSignal(0);
        }
        ImGui::SameLine();
        if (ImGui::Button("Follow me")) {
            MultiplayerManager::SendSignal(1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Wait")) {
            MultiplayerManager::SendSignal(2);
        }
        ImGui::SameLine();
        if (ImGui::Button("Help")) {
            MultiplayerManager::SendSignal(3);
        }
        ImGui::EndDisabled();
    }).PreFunc([](WidgetInfo& info) { info.isHidden = IsAnchorModeSelected(); });

    AddWidget(path,
              "Players are visible only when they are in the same scene and room. "
              "The server relays movement, pose and appearance; it does not modify your save file.",
              WIDGET_TEXT)
        .PreFunc([](WidgetInfo& info) {
            info.name = IsAnchorModeSelected()
                            ? "For visibility, both players must use the same Anchor room, scene and Link age. Saves "
                              "remain local."
                            : "Legacy players are visible in the same scene and in-game room. Saves remain local.";
        });
}

} // namespace SohGui
#endif

namespace SohGui {

using namespace UIWidgets;

#if defined(__ANDROID__)
static constexpr uint32_t RESPONSIVE_MULTIPLAYER_COLUMNS = 1;
#else
static constexpr uint32_t RESPONSIVE_MULTIPLAYER_COLUMNS = 2;
#endif

static float MultiplayerFormWidth() {
    const float available = ImGui::GetContentRegionAvail().x;
    return std::clamp(available * 0.56f, ImGui::GetFontSize() * 13.0f, ImGui::GetFontSize() * 34.0f);
}

static void SaveMultiplayerSettings() {
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

static void DrawCVarString(const char* label, const char* id, const char* cvar, const char* placeholder,
                           const char* defaultValue, float width) {
    ImGui::TextUnformatted(label);
    CVarInputString(id, cvar,
                    InputOptions()
                        .Color(THEME_COLOR)
                        .PlaceholderText(placeholder)
                        .DefaultValue(defaultValue)
                        .Size(ImVec2(width, 0))
                        .LabelPosition(LabelPositions::None));
}

static void DrawHostAndPort(const char* hostCVar, const char* portCVar, const char* defaultHost, float formWidth,
                            const char* idSuffix) {
    const float portWidth = std::clamp(formWidth * 0.25f, ImGui::GetFontSize() * 5.0f,
                                       ImGui::GetFontSize() * 7.0f);
    const float separatorWidth = ImGui::CalcTextSize(":").x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    const float hostWidth = std::max(ImGui::GetFontSize() * 7.0f, formWidth - portWidth - separatorWidth);
    const std::string hostId = std::string("##MultiplayerHost") + idSuffix;
    const std::string portId = std::string("##MultiplayerPort") + idSuffix;

    ImGui::TextUnformatted("Host & Port");
    CVarInputString(hostId.c_str(), hostCVar,
                    InputOptions()
                        .Color(THEME_COLOR)
                        .PlaceholderText(defaultHost)
                        .DefaultValue(defaultHost)
                        .Size(ImVec2(hostWidth, 0))
                        .LabelPosition(LabelPositions::None));
    ImGui::SameLine();
    ImGui::TextUnformatted(":");
    ImGui::SameLine();
    CVarInputInt(portId.c_str(), portCVar,
                 InputOptions()
                     .Color(THEME_COLOR)
                     .PlaceholderText("43383")
                     .DefaultValue("43383")
                     .Size(ImVec2(portWidth, 0))
                     .LabelPosition(LabelPositions::None));
}

static void DrawAnchorMenu(WidgetInfo& info) {
    (void)info;
    const float formWidth = MultiplayerFormWidth();
    const float buttonWidth = (formWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    const bool enabled = CVarGetInteger(CVAR_GENERAL("Multiplayer.Enabled"), 1) != 0 &&
                         CVarGetInteger(CVAR_GENERAL("Multiplayer.AnchorMode"), 1) != 0;
    const char* host = CVarGetString(CVAR_GENERAL("Multiplayer.AnchorHost"), "anchor.hm64.org");
    const char* name = CVarGetString(CVAR_GENERAL("Multiplayer.PlayerName"), "AndroidPlayer");
    const char* room = CVarGetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "soh-global");
    const int port = CVarGetInteger(CVAR_GENERAL("Multiplayer.AnchorPort"), 43383);
    const bool formValid = host != nullptr && host[0] != '\0' && name != nullptr && name[0] != '\0' &&
                           room != nullptr && room[0] != '\0' && port > 0 && port <= 65535;

    ImGui::SeparatorText("Connection Settings");
    ImGui::BeginDisabled(enabled);
    DrawHostAndPort(CVAR_GENERAL("Multiplayer.AnchorHost"), CVAR_GENERAL("Multiplayer.AnchorPort"),
                    "anchor.hm64.org", formWidth, "Anchor");

    ImGui::TextUnformatted("Name & Color");
    CVarColorPicker("##MultiplayerPlayerColor", CVAR_GENERAL("Multiplayer.PlayerColor"),
                    { 100, 255, 100, 255 });
    ImGui::SameLine();
    const float nameWidth = std::max(ImGui::GetFontSize() * 8.0f,
                                     formWidth - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
    CVarInputString("##MultiplayerAnchorName", CVAR_GENERAL("Multiplayer.PlayerName"),
                    InputOptions()
                        .Color(THEME_COLOR)
                        .PlaceholderText("AndroidPlayer")
                        .DefaultValue("AndroidPlayer")
                        .Size(ImVec2(nameWidth, 0))
                        .LabelPosition(LabelPositions::None));

    DrawCVarString("Room ID", "##MultiplayerAnchorRoom", CVAR_GENERAL("Multiplayer.AnchorRoomId"),
                   "soh-global", "soh-global", formWidth);
    DrawCVarString("Team ID (Items & Flags Shared)", "##MultiplayerAnchorTeam",
                   CVAR_GENERAL("Multiplayer.AnchorTeamId"), "default", "default", formWidth);

    ImGui::Spacing();
    if (Button("Restore Defaults", ButtonOptions().Size(ImVec2(buttonWidth, 0)).Color(Colors::Red))) {
        CVarSetString(CVAR_GENERAL("Multiplayer.AnchorHost"), "anchor.hm64.org");
        CVarSetInteger(CVAR_GENERAL("Multiplayer.AnchorPort"), 43383);
        CVarSetString(CVAR_GENERAL("Multiplayer.AnchorTeamId"), "default");
        CVarSetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "");
        CVarSetString(CVAR_GENERAL("Multiplayer.PlayerName"), "");
        SaveMultiplayerSettings();
    }
    ImGui::SameLine();
    if (Button("Global Room", ButtonOptions()
                                  .Size(ImVec2(buttonWidth, 0))
                                  .Color(Colors::Blue)
                                  .Tooltip("Public Anchor room with item and flag sharing disabled."))) {
        CVarSetString(CVAR_GENERAL("Multiplayer.AnchorHost"), "anchor.hm64.org");
        CVarSetInteger(CVAR_GENERAL("Multiplayer.AnchorPort"), 43383);
        CVarSetString(CVAR_GENERAL("Multiplayer.AnchorTeamId"), "default");
        CVarSetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "soh-global");
        SaveMultiplayerSettings();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::BeginDisabled(!formValid);
    if (Button(enabled ? "Disable" : "Enable",
               ButtonOptions().Size(ImVec2(formWidth, 0)).Color(enabled ? Colors::Red : Colors::Green))) {
        if (enabled) {
            CVarSetInteger(CVAR_GENERAL("Multiplayer.Enabled"), 0);
        } else {
            CVarSetInteger(CVAR_GENERAL("Multiplayer.AnchorMode"), 1);
            CVarSetInteger(CVAR_GENERAL("Multiplayer.Enabled"), 1);
        }
        SaveMultiplayerSettings();
        MultiplayerManager::Reconnect();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextWrapped("Use the same Room ID as the PC Ackbar version. The room is created automatically when the "
                       "first player connects.");
}

static void DrawStatusMenu(WidgetInfo& info) {
    (void)info;
    const char* host = CVarGetString(CVAR_GENERAL("Multiplayer.AnchorHost"), "anchor.hm64.org");
    const int port = CVarGetInteger(CVAR_GENERAL("Multiplayer.AnchorPort"), 43383);
    const char* room = CVarGetString(CVAR_GENERAL("Multiplayer.AnchorRoomId"), "soh-global");

    ImGui::SeparatorText("Connection Status");
    if (!MultiplayerManager::IsEnabled()) {
        ImGui::TextColored(ImVec4(0.8f, 0.55f, 0.25f, 1.0f), "Disabled");
    } else if (MultiplayerManager::IsConnected()) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.45f, 1.0f), "Connected");
    } else if (MultiplayerManager::IsConnecting()) {
        ImGui::TextColored(ImVec4(0.35f, 0.7f, 1.0f, 1.0f), "Connecting...");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.45f, 0.35f, 1.0f), "Waiting for connection");
    }

    ImGui::TextUnformatted("Mode: Anchor");
    ImGui::Text("Server: %s:%d", host != nullptr ? host : "", port);
    ImGui::Text("Room ID: %s", room != nullptr ? room : "");
    ImGui::Text("Remote players: %zu", MultiplayerManager::GetRemotePlayerCount());
    const int latency = MultiplayerManager::GetLatencyMs();
    const std::string latencyText = latency >= 0 ? std::to_string(latency) + " ms" : "--";
    ImGui::Text("Latency: %s", latencyText.c_str());

    ImGui::Spacing();
    if (Button("Reconnect", ButtonOptions().Size(ImVec2(MultiplayerFormWidth(), 0)).Color(THEME_COLOR))) {
        SaveMultiplayerSettings();
        MultiplayerManager::Reconnect();
    }

    ImGui::SeparatorText("Player List");
    const auto playerNames = MultiplayerManager::GetRemotePlayerNames();
    if (playerNames.empty()) {
        ImGui::TextDisabled("No remote players");
    } else {
        for (const auto& playerName : playerNames) {
            ImGui::BulletText("%s", playerName.c_str());
        }
    }

}

#ifdef ENABLE_REMOTE_CONTROL
static void DrawCrowdControlMenu(WidgetInfo&) {
    CrowdControl* crowdControl = CrowdControl::Instance;
    if (crowdControl == nullptr) {
        ImGui::TextDisabled("Crowd Control unavailable");
        return;
    }

    ImGui::SeparatorText("About Crowd Control");
    ImGui::TextWrapped("Crowd Control is a platform that allows viewers to interact with a streamer's game in real time.");
    ImGui::Spacing();
    ImGui::TextWrapped("Please head over to www.crowdcontrol.live for more information!");

    ImGui::SeparatorText("Connect to Crowd Control");
    const float width = MultiplayerFormWidth();
    const float portWidth = std::clamp(width * 0.28f, ImGui::GetFontSize() * 5.0f, ImGui::GetFontSize() * 7.0f);
    const float hostWidth = std::max(ImGui::GetFontSize() * 7.0f,
                                     width - portWidth - ImGui::CalcTextSize(":").x - ImGui::GetStyle().ItemSpacing.x * 2.0f);

    ImGui::BeginDisabled(crowdControl->isEnabled);
    ImGui::TextUnformatted("Host & Port");
    CVarInputString("##CrowdControlHost", CVAR_REMOTE_CROWD_CONTROL("Host"),
                    InputOptions().Color(THEME_COLOR).DefaultValue("127.0.0.1").PlaceholderText("127.0.0.1")
                        .Size(ImVec2(hostWidth, 0)).LabelPosition(LabelPositions::None));
    ImGui::SameLine();
    ImGui::TextUnformatted(":");
    ImGui::SameLine();
    CVarInputInt("##CrowdControlPort", CVAR_REMOTE_CROWD_CONTROL("Port"),
                 InputOptions().Color(THEME_COLOR).DefaultValue("43384").PlaceholderText("43384")
                     .Size(ImVec2(portWidth, 0)).LabelPosition(LabelPositions::None));
    ImGui::EndDisabled();

    const char* host = CVarGetString(CVAR_REMOTE_CROWD_CONTROL("Host"), "127.0.0.1");
    const int port = CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("Port"), 43384);
    const bool valid = host != nullptr && host[0] != '\0' && port > 1024 && port < 65535;
    ImGui::BeginDisabled(!valid);
    if (Button(crowdControl->isEnabled ? "Disable Crowd Control" : "Enable Crowd Control",
               ButtonOptions().Size(ImVec2(width, 0)).Color(crowdControl->isEnabled ? Colors::Red : Colors::Green))) {
        if (crowdControl->isEnabled) {
            CVarClear(CVAR_REMOTE_CROWD_CONTROL("Enabled"));
            crowdControl->Disable();
        } else {
            CVarSetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 1);
            crowdControl->Enable();
        }
        SaveMultiplayerSettings();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::SeparatorText("Additional Settings");
    bool enemyNameTags = CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("EnemyNameTags"), 1) != 0;
    if (ImGui::Checkbox("Enemy Name Tags", &enemyNameTags)) {
        CVarSetInteger(CVAR_REMOTE_CROWD_CONTROL("EnemyNameTags"), enemyNameTags ? 1 : 0);
        SaveMultiplayerSettings();
    }
    bool spawnedEnemiesIgnoredIngame = CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("SpawnedEnemiesIgnoredIngame"), 0) != 0;
    if (ImGui::Checkbox("Spawned Enemies Ignored Ingame", &spawnedEnemiesIgnoredIngame)) {
        CVarSetInteger(CVAR_REMOTE_CROWD_CONTROL("SpawnedEnemiesIgnoredIngame"), spawnedEnemiesIgnoredIngame ? 1 : 0);
        SaveMultiplayerSettings();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Crowd Control Status");
    if (crowdControl->isConnected) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.45f, 1.0f), "Connected");
    } else if (crowdControl->isEnabled) {
        ImGui::TextColored(ImVec4(0.35f, 0.7f, 1.0f, 1.0f), "Connecting...");
    } else {
        ImGui::TextDisabled("Disabled");
    }
}
#endif

void SohMenu::AddMenuMultiplayer() {
    AddMenuEntry("Multiplayer", CVAR_SETTING("Menu.MultiplayerSidebarSection"));

    WidgetPath anchorPath = { "Multiplayer", "Anchor", SECTION_COLUMN_1 };
    AddSidebarEntry("Multiplayer", anchorPath.sidebarName, RESPONSIVE_MULTIPLAYER_COLUMNS);
    AddWidget(anchorPath, "Anchor", WIDGET_CUSTOM).CustomFunction(DrawAnchorMenu);

    WidgetPath statusPath = { "Multiplayer", "Status", SECTION_COLUMN_1 };
    AddSidebarEntry("Multiplayer", statusPath.sidebarName, RESPONSIVE_MULTIPLAYER_COLUMNS);
    AddWidget(statusPath, "Status", WIDGET_CUSTOM).CustomFunction(DrawStatusMenu);

#ifdef ENABLE_REMOTE_CONTROL
    WidgetPath crowdControlPath = { "Multiplayer", "Crowd Control", SECTION_COLUMN_1 };
    AddSidebarEntry("Multiplayer", crowdControlPath.sidebarName, RESPONSIVE_MULTIPLAYER_COLUMNS);
    AddWidget(crowdControlPath, "Crowd Control", WIDGET_CUSTOM).CustomFunction(DrawCrowdControlMenu);
#endif
}

} // namespace SohGui
