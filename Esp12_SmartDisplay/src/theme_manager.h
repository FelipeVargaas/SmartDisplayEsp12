#pragma once

#include <Arduino.h>

#include "theme_types.h"

void themeManagerLoad();
ThemeId themeManagerGetActive();
bool themeManagerSetActive(ThemeId theme);
bool themeManagerSetByKey(const String& key);
bool themeManagerThemeFromKey(const String& key, ThemeId& theme);
const char* themeManagerGetKey(ThemeId theme);
const char* themeManagerGetFriendlyName(ThemeId theme);
