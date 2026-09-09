#include "SohMenu.h"
#include "MenuLocalization.h"

namespace SohGui {

using namespace UIWidgets;

static const std::unordered_map<int32_t, const char*> menuLanguageOptions = {
    { MENU_LANGUAGE_ENGLISH, "English" },
    { MENU_LANGUAGE_PORTUGUESE, "Português" },
    { MENU_LANGUAGE_SPANISH, "Español" },
};

void SohMenu::AddMenuLanguage() {
    // Keep this entry immediately after Dev Tools in the top navigation.
    AddMenuEntry("Language", CVAR_SETTING("Menu.LanguageSidebarSection"));
    AddSidebarEntry("Language", "Language", 1);
    WidgetPath path = { "Language", "Language", SECTION_COLUMN_1 };

    AddWidget(path, "##MenuLanguage", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_SETTING("Menu.Language"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(menuLanguageOptions)
                     .DefaultIndex(MENU_LANGUAGE_ENGLISH)
                     .LabelPosition(LabelPositions::None)
                     .ComponentAlignment(ComponentAlignments::Left)
                     .Tooltip("Changes every supported menu label immediately. The selection is saved."));
}

} // namespace SohGui
