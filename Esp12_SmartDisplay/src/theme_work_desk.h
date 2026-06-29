#pragma once

#include <Arduino.h>

bool themeWorkDeskShowNotification(
  const char* appName,
  const char* sender,
  const char* title,
  const char* timeText,
  const char* accent,
  unsigned long durationMs);
