#include "reset_marker.h"

#include <ESP8266WiFi.h>
#include <string.h>

namespace
{
const uint32_t RESET_MARKER_MAGIC = 0x52535432UL;
const uint32_t CHECKPOINT_MARKER_MAGIC = 0x43485032UL;
const uint32_t RESET_MARKER_CHECK = 0x51A7C0DEUL;
const uint32_t RESET_MARKER_RTC_OFFSET_WORDS = 40; // Depois do boot_guard e longe do comando OTA.
const uint32_t CHECKPOINT_MARKER_RTC_OFFSET_WORDS = 52;

struct RtcResetMarker
{
  uint32_t magic;
  char reason[24];
  uint32_t check;
};

uint32_t checksum(const RtcResetMarker& marker)
{
  uint32_t check = RESET_MARKER_CHECK ^ marker.magic;
  for (unsigned int i = 0; i < sizeof(marker.reason); i++) check = (check << 5) ^ (check >> 27) ^ marker.reason[i];
  return check;
}

void clearMarker()
{
  RtcResetMarker marker = {};
  ESP.rtcUserMemoryWrite(RESET_MARKER_RTC_OFFSET_WORDS, reinterpret_cast<uint32_t*>(&marker), sizeof(marker));
}

void writeMarker(uint32_t offsetWords, uint32_t magic, const char* reason)
{
  RtcResetMarker marker = {};
  marker.magic = magic;
  strncpy(marker.reason, reason ? reason : "unknown", sizeof(marker.reason) - 1);
  marker.reason[sizeof(marker.reason) - 1] = '\0';
  marker.check = checksum(marker);
  ESP.rtcUserMemoryWrite(offsetWords, reinterpret_cast<uint32_t*>(&marker), sizeof(marker));
}

String readMarker(uint32_t offsetWords, uint32_t magic)
{
  RtcResetMarker marker = {};
  if (!ESP.rtcUserMemoryRead(offsetWords, reinterpret_cast<uint32_t*>(&marker), sizeof(marker))) return "unknown";
  if (marker.magic != magic || marker.check != checksum(marker)) return "unknown";
  String reason = marker.reason;
  reason.trim();
  return reason.length() ? reason : "unknown";
}
}

void resetMarkerSet(const char* reason)
{
  writeMarker(RESET_MARKER_RTC_OFFSET_WORDS, RESET_MARKER_MAGIC, reason);
}

String resetMarkerConsume()
{
  String reason = readMarker(RESET_MARKER_RTC_OFFSET_WORDS, RESET_MARKER_MAGIC);
  clearMarker();
  return reason;
}

void resetMarkerCheckpoint(const char* checkpoint)
{
  writeMarker(CHECKPOINT_MARKER_RTC_OFFSET_WORDS, CHECKPOINT_MARKER_MAGIC, checkpoint);
}

String resetMarkerReadCheckpoint()
{
  return readMarker(CHECKPOINT_MARKER_RTC_OFFSET_WORDS, CHECKPOINT_MARKER_MAGIC);
}
