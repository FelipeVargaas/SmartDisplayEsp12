#include "boot_guard.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace
{
const uint32_t BOOT_GUARD_MAGIC = 0x53444732UL;
const uint32_t BOOT_GUARD_CHECK = 0xB007CAFEUL;
const uint32_t RTC_OFFSET_WORDS = 32; // Keeps clear of the 128-byte OTA boot command area.
const uint32_t SAFE_MODE_AFTER_FAILED_STARTS = 3;
const unsigned long HEALTHY_BOOT_DELAY_MS = 15000UL;

struct RtcBootGuard
{
  uint32_t magic;
  uint32_t pending;
  uint32_t failedStarts;
  uint32_t check;
};

unsigned long bootStartedAt = 0;
bool healthyBootRecorded = false;

bool isValid(const RtcBootGuard& guard)
{
  return guard.magic == BOOT_GUARD_MAGIC && guard.check == (BOOT_GUARD_CHECK ^ guard.pending ^ guard.failedStarts);
}

void save(bool pending, uint32_t failedStarts)
{
  RtcBootGuard guard = {BOOT_GUARD_MAGIC, pending ? 1UL : 0UL, failedStarts, 0};
  guard.check = BOOT_GUARD_CHECK ^ guard.pending ^ guard.failedStarts;
  ESP.rtcUserMemoryWrite(RTC_OFFSET_WORDS, reinterpret_cast<uint32_t*>(&guard), sizeof(guard));
}
}

bool bootGuardBegin()
{
  RtcBootGuard previous = {};
  uint32_t failedStarts = 1;
  if (ESP.rtcUserMemoryRead(RTC_OFFSET_WORDS, reinterpret_cast<uint32_t*>(&previous), sizeof(previous)) && isValid(previous))
  {
    if (previous.pending)
      failedStarts = previous.failedStarts < 255 ? previous.failedStarts + 1 : previous.failedStarts;
  }

  save(true, failedStarts);
  bootStartedAt = millis();
  return failedStarts >= SAFE_MODE_AFTER_FAILED_STARTS;
}

void bootGuardUpdate()
{
  if (healthyBootRecorded || millis() - bootStartedAt < HEALTHY_BOOT_DELAY_MS) return;
  save(false, 0);
  healthyBootRecorded = true;
}
