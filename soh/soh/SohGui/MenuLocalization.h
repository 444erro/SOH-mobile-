#ifndef SOH_MENU_LOCALIZATION_H
#define SOH_MENU_LOCALIZATION_H

#include <string>

namespace SohGui {

enum MenuLanguage {
    MENU_LANGUAGE_ENGLISH = 0,
    MENU_LANGUAGE_PORTUGUESE = 1,
    MENU_LANGUAGE_SPANISH = 2,
};

// Translates display text only. Internal menu keys and CVar names remain stable in English.
std::string LocalizeMenuText(const std::string& text);

} // namespace SohGui

#endif
