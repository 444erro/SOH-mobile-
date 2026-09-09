#include "MenuLocalization.h"

#include "soh/cvar_prefixes.h"
#include "public/bridge/consolevariablebridge.h"
#include "variables.h"

#include <cctype>
#include <unordered_map>

namespace SohGui {

using Dictionary = std::unordered_map<std::string, std::string>;

static const Dictionary sPortuguesePhrases = {
    { "Settings", "Configurações" },
    { "Enhancements", "Melhorias" },
    { "Randomizer", "Randomizador" },
    { "Seed Settings", "Configurações da seed" },
    { "Starting Items", "Itens iniciais" },
    { "Locations", "Locais" },
    { "Tricks/Glitches", "Truques/Glitches" },
    { "Open Randomizer Settings", "Abrir configurações do Randomizador" },
    { "Open Starting Items", "Abrir itens iniciais" },
    { "Open Excluded Locations", "Abrir locais excluídos" },
    { "Open Tricks/Glitches", "Abrir truques/glitches" },
    { "Multiplayer", "Multijogador" },
    { "Network", "Rede" },
    { "Dev Tools", "Ferramentas" },
    { "Language", "Idioma" },
    { "Menu Language", "Idioma dos menus" },
    { "General", "Geral" },
    { "Graphics", "Gráficos" },
    { "Audio", "Áudio" },
    { "Controls", "Controles" },
    { "HUD", "HUD" },
    { "Touch HUD Layout", "Layout do HUD touch" },
    { "Edit Touch HUD", "Editar HUD touch" },
    { "Reset Touch HUD Layout", "Resetar layout do HUD touch" },
    { "Notifications", "Notificações" },
    { "Position", "Posição" },
    { "Duration (seconds):", "Duração (segundos):" },
    { "Background Opacity", "Opacidade do fundo" },
    { "Size:", "Tamanho:" },
    { "Test Notification", "Testar notificação" },
    { "Mute Notification Sound", "Silenciar som das notificações" },
    { "Accessibility", "Acessibilidade" },
    { "Gameplay", "Jogabilidade" },
    { "Cosmetics", "Aparência" },
    { "Cheats", "Trapaças" },
    { "Developer Tools", "Ferramentas de desenvolvimento" },
    { "Map Select Button Combination:", "Combinação para seleção de mapa:" },
    { "No Clip Button Combination:", "Combinação para atravessar paredes:" },
    { "Log Level", "Nível de log" },
    { "Menu Settings", "Configurações do menu" },
    { "Mod Menu", "Menu de mods" },
    { "Menu Theme", "Tema do menu" },
    { "Menu Background Opacity", "Opacidade do fundo do menu" },
    { "Search In Sidebar", "Pesquisar na barra lateral" },
    { "Search Input Autofocus", "Foco automático da pesquisa" },
    { "General Settings", "Configurações gerais" },
    { "Cursor Always Visible", "Cursor sempre visível" },
    { "Open App Files Folder", "Abrir pasta de arquivos" },
    { "Import Mod Files", "Importar arquivos de mod" },
    { "Boot Sequence", "Sequência de inicialização" },
    { "Languages", "Idiomas" },
    { "Translate Title Screen", "Traduzir tela de título" },
    { "Text to Speech", "Texto para voz" },
    { "ImGui Menu Scaling", "Tamanho dos menus" },
    { "Restore Defaults", "Restaurar padrões" },
    { "Host & Port", "Servidor e porta" },
    { "Name and Color", "Nome e cor" },
    { "Room ID", "ID da sala" },
    { "Team ID (Items & Flags Shared)", "ID da equipe (itens e eventos compartilhados)" },
    { "Connection Status", "Estado da conexão" },
    { "Crowd Control", "Crowd Control" },
    { "Clear Search", "Limpar pesquisa" },
    { "No results found", "Nenhum resultado encontrado" },
    { "Start typing to see results.", "Comece a digitar para ver resultados." },
    { "Compact", "Compacto" },
    { "Automatic", "Automático" },
    { "Comfortable", "Confortável" },
    { "Large", "Grande" },
    { "Small", "Pequeno" },
    { "Normal", "Normal" },
    { "Default", "Padrão" },
    { "Disabled", "Desativado" },
    { "Enabled", "Ativado" },
    { "None", "Nenhum" },
    { "Always", "Sempre" },
    { "Never", "Nunca" },
    { "All", "Todos" },
    { "On", "Ligado" },
    { "Off", "Desligado" },
    { "Top Left", "Superior esquerdo" },
    { "Top Right", "Superior direito" },
    { "Bottom Left", "Inferior esquerdo" },
    { "Bottom Right", "Inferior direito" },
};

static const Dictionary sSpanishPhrases = {
    { "Settings", "Configuración" },
    { "Enhancements", "Mejoras" },
    { "Randomizer", "Aleatorizador" },
    { "Seed Settings", "Configuración de semilla" },
    { "Starting Items", "Objetos iniciales" },
    { "Locations", "Ubicaciones" },
    { "Tricks/Glitches", "Trucos/Glitches" },
    { "Open Randomizer Settings", "Abrir configuración del aleatorizador" },
    { "Open Starting Items", "Abrir objetos iniciales" },
    { "Open Excluded Locations", "Abrir ubicaciones excluidas" },
    { "Open Tricks/Glitches", "Abrir trucos/glitches" },
    { "Multiplayer", "Multijugador" },
    { "Network", "Red" },
    { "Dev Tools", "Herramientas" },
    { "Language", "Idioma" },
    { "Menu Language", "Idioma de los menús" },
    { "General", "General" },
    { "Graphics", "Gráficos" },
    { "Audio", "Audio" },
    { "Controls", "Controles" },
    { "HUD", "HUD" },
    { "Touch HUD Layout", "Diseño del HUD táctil" },
    { "Edit Touch HUD", "Editar HUD táctil" },
    { "Reset Touch HUD Layout", "Restaurar diseño del HUD táctil" },
    { "Notifications", "Notificaciones" },
    { "Position", "Posición" },
    { "Duration (seconds):", "Duración (segundos):" },
    { "Background Opacity", "Opacidad del fondo" },
    { "Size:", "Tamaño:" },
    { "Test Notification", "Probar notificación" },
    { "Mute Notification Sound", "Silenciar sonido de notificaciones" },
    { "Accessibility", "Accesibilidad" },
    { "Gameplay", "Jugabilidad" },
    { "Cosmetics", "Apariencia" },
    { "Cheats", "Trucos" },
    { "Developer Tools", "Herramientas de desarrollo" },
    { "Map Select Button Combination:", "Combinación para selección de mapa:" },
    { "No Clip Button Combination:", "Combinación para atravesar paredes:" },
    { "Log Level", "Nivel de registro" },
    { "Menu Settings", "Configuración del menú" },
    { "Mod Menu", "Menú de mods" },
    { "Menu Theme", "Tema del menú" },
    { "Menu Background Opacity", "Opacidad del fondo del menú" },
    { "Search In Sidebar", "Buscar en la barra lateral" },
    { "Search Input Autofocus", "Enfoque automático de búsqueda" },
    { "General Settings", "Configuración general" },
    { "Cursor Always Visible", "Cursor siempre visible" },
    { "Open App Files Folder", "Abrir carpeta de archivos" },
    { "Import Mod Files", "Importar archivos de mod" },
    { "Boot Sequence", "Secuencia de inicio" },
    { "Languages", "Idiomas" },
    { "Translate Title Screen", "Traducir pantalla de título" },
    { "Text to Speech", "Texto a voz" },
    { "ImGui Menu Scaling", "Tamaño de los menús" },
    { "Restore Defaults", "Restaurar valores" },
    { "Host & Port", "Servidor y puerto" },
    { "Name and Color", "Nombre y color" },
    { "Room ID", "ID de sala" },
    { "Team ID (Items & Flags Shared)", "ID de equipo (objetos y eventos compartidos)" },
    { "Connection Status", "Estado de conexión" },
    { "Crowd Control", "Crowd Control" },
    { "Clear Search", "Limpiar búsqueda" },
    { "No results found", "No se encontraron resultados" },
    { "Start typing to see results.", "Empieza a escribir para ver resultados." },
    { "Compact", "Compacto" },
    { "Automatic", "Automático" },
    { "Comfortable", "Cómodo" },
    { "Large", "Grande" },
    { "Small", "Pequeño" },
    { "Normal", "Normal" },
    { "Default", "Predeterminado" },
    { "Disabled", "Desactivado" },
    { "Enabled", "Activado" },
    { "None", "Ninguno" },
    { "Always", "Siempre" },
    { "Never", "Nunca" },
    { "All", "Todos" },
    { "On", "Activado" },
    { "Off", "Desactivado" },
    { "Top Left", "Superior izquierda" },
    { "Top Right", "Superior derecha" },
    { "Bottom Left", "Inferior izquierda" },
    { "Bottom Right", "Inferior derecha" },
};

static const Dictionary sPortugueseWords = {
    { "advanced", "avançado" }, { "allow", "permitir" }, { "animation", "animação" },
    { "animations", "animações" }, { "background", "fundo" }, { "button", "botão" },
    { "camera", "câmera" }, { "change", "alterar" }, { "color", "cor" }, { "colors", "cores" },
    { "connection", "conexão" }, { "controller", "controle" }, { "debug", "depuração" },
    { "disable", "desativar" }, { "disabled", "desativado" }, { "display", "exibição" },
    { "enable", "ativar" }, { "enabled", "ativado" }, { "enemy", "inimigo" },
    { "file", "arquivo" }, { "files", "arquivos" }, { "filter", "filtro" }, { "frame", "quadro" },
    { "fullscreen", "tela cheia" }, { "game", "jogo" }, { "graphics", "gráficos" },
    { "hide", "ocultar" }, { "input", "entrada" }, { "item", "item" }, { "items", "itens" },
    { "language", "idioma" }, { "limit", "limite" }, { "menu", "menu" }, { "mode", "modo" },
    { "movement", "movimento" }, { "name", "nome" }, { "notification", "notificação" },
    { "notifications", "notificações" }, { "opacity", "opacidade" }, { "open", "abrir" },
    { "player", "jogador" }, { "players", "jogadores" }, { "position", "posição" },
    { "random", "aleatório" }, { "reset", "reiniciar" }, { "resolution", "resolução" },
    { "save", "salvar" }, { "screen", "tela" }, { "search", "pesquisa" }, { "show", "mostrar" },
    { "size", "tamanho" }, { "speed", "velocidade" }, { "status", "estado" }, { "text", "texto" },
    { "theme", "tema" }, { "tools", "ferramentas" }, { "window", "janela" }, { "world", "mundo" },
};

static const Dictionary sSpanishWords = {
    { "advanced", "avanzado" }, { "allow", "permitir" }, { "animation", "animación" },
    { "animations", "animaciones" }, { "background", "fondo" }, { "button", "botón" },
    { "camera", "cámara" }, { "change", "cambiar" }, { "color", "color" }, { "colors", "colores" },
    { "connection", "conexión" }, { "controller", "control" }, { "debug", "depuración" },
    { "disable", "desactivar" }, { "disabled", "desactivado" }, { "display", "pantalla" },
    { "enable", "activar" }, { "enabled", "activado" }, { "enemy", "enemigo" },
    { "file", "archivo" }, { "files", "archivos" }, { "filter", "filtro" }, { "frame", "fotograma" },
    { "fullscreen", "pantalla completa" }, { "game", "juego" }, { "graphics", "gráficos" },
    { "hide", "ocultar" }, { "input", "entrada" }, { "item", "objeto" }, { "items", "objetos" },
    { "language", "idioma" }, { "limit", "límite" }, { "menu", "menú" }, { "mode", "modo" },
    { "movement", "movimiento" }, { "name", "nombre" }, { "notification", "notificación" },
    { "notifications", "notificaciones" }, { "opacity", "opacidad" }, { "open", "abrir" },
    { "player", "jugador" }, { "players", "jugadores" }, { "position", "posición" },
    { "random", "aleatorio" }, { "reset", "reiniciar" }, { "resolution", "resolución" },
    { "save", "guardar" }, { "screen", "pantalla" }, { "search", "búsqueda" }, { "show", "mostrar" },
    { "size", "tamaño" }, { "speed", "velocidad" }, { "status", "estado" }, { "text", "texto" },
    { "theme", "tema" }, { "tools", "herramientas" }, { "window", "ventana" }, { "world", "mundo" },
};

static std::string TranslateWords(const std::string& text, const Dictionary& words) {
    std::string result;
    size_t cursor = 0;
    while (cursor < text.size()) {
        if (!std::isalpha(static_cast<unsigned char>(text[cursor]))) {
            result += text[cursor++];
            continue;
        }
        const size_t start = cursor;
        while (cursor < text.size() && std::isalpha(static_cast<unsigned char>(text[cursor]))) {
            cursor++;
        }
        std::string word = text.substr(start, cursor - start);
        std::string key = word;
        for (char& character : key) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        auto translated = words.find(key);
        if (translated == words.end()) {
            result += word;
            continue;
        }
        std::string replacement = translated->second;
        if (!word.empty() && std::isupper(static_cast<unsigned char>(word.front())) && !replacement.empty()) {
            replacement.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(replacement.front())));
        }
        result += replacement;
    }
    return result;
}

std::string LocalizeMenuText(const std::string& text) {
    const int language = CVarGetInteger(CVAR_SETTING("Menu.Language"), MENU_LANGUAGE_ENGLISH);
    if (language == MENU_LANGUAGE_ENGLISH || text.empty()) {
        return text;
    }

    const Dictionary& phrases = language == MENU_LANGUAGE_PORTUGUESE ? sPortuguesePhrases : sSpanishPhrases;
    const auto phrase = phrases.find(text);
    if (phrase != phrases.end()) {
        return phrase->second;
    }
    return TranslateWords(text, language == MENU_LANGUAGE_PORTUGUESE ? sPortugueseWords : sSpanishWords);
}

} // namespace SohGui
