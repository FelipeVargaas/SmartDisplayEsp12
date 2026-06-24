#include "theme_manager.h"

#include <EEPROM.h>

#include "config.h"

static ThemeId activeTheme = THEME_PC_MONITOR;

static bool themeIsValid(ThemeId theme)
{
  return theme < THEME_COUNT;
}

void themeManagerLoad()
{
  EEPROM.begin(EEPROM_SIZE);
  uint8_t magic = EEPROM.read(THEME_STORAGE_MAGIC_ADDR);
  ThemeId storedTheme = static_cast<ThemeId>(EEPROM.read(THEME_STORAGE_ADDR));
  EEPROM.end();

  if (magic == THEME_STORAGE_MAGIC && themeIsValid(storedTheme))
  {
    activeTheme = storedTheme;
    return;
  }

  activeTheme = THEME_PC_MONITOR;
  themeManagerSetActive(activeTheme);
}

ThemeId themeManagerGetActive()
{
  return activeTheme;
}

bool themeManagerSetActive(ThemeId theme)
{
  if (!themeIsValid(theme)) return false;
  activeTheme = theme;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(THEME_STORAGE_ADDR, static_cast<uint8_t>(theme));
  EEPROM.write(THEME_STORAGE_MAGIC_ADDR, THEME_STORAGE_MAGIC);
  bool saved = EEPROM.commit();
  EEPROM.end();
  return saved;
}

bool themeManagerThemeFromKey(const String& key, ThemeId& theme)
{
  if (key == "pc_monitor") theme = THEME_PC_MONITOR;
  else if (key == "work_desk") theme = THEME_WORK_DESK;
  else if (key == "gamer") theme = THEME_GAMER;
  else if (key == "minimal_clock") theme = THEME_MINIMAL_CLOCK;
  else if (key == "animation") theme = THEME_ANIMATION;
  else return false;
  return true;
}

bool themeManagerSetByKey(const String& key)
{
  ThemeId theme;
  return themeManagerThemeFromKey(key, theme) && themeManagerSetActive(theme);
}

const char* themeManagerGetKey(ThemeId theme)
{
  switch (theme)
  {
    case THEME_WORK_DESK: return "work_desk";
    case THEME_GAMER: return "gamer";
    case THEME_MINIMAL_CLOCK: return "minimal_clock";
    case THEME_ANIMATION: return "animation";
    case THEME_PC_MONITOR:
    default: return "pc_monitor";
  }
}

const char* themeManagerGetFriendlyName(ThemeId theme)
{
  switch (theme)
  {
    case THEME_WORK_DESK: return "Work Desk";
    case THEME_GAMER: return "Gamer";
    case THEME_MINIMAL_CLOCK: return "Minimal Clock";
    case THEME_ANIMATION: return "Animation";
    case THEME_PC_MONITOR:
    default: return "PC Monitor";
  }
}
