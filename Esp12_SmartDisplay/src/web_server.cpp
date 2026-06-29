#include "web_server.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <math.h>
#include <string.h>

#include "animation_image.h"
#include "app_state.h"
#include "config.h"
#include "metrics.h"
#include "ota_update.h"
#include "reset_marker.h"
#include "theme_manager.h"
#include "theme_render.h"
#include "web_pages.h"
#include "wifi_manager.h"

static ESP8266WebServer server(80);

namespace
{
const int METRICS_MIN_FREE_HEAP = 12000;
const int METRICS_MIN_MAX_BLOCK = 8000;
const int STATUS_MIN_FREE_HEAP = 14000;
const int STATUS_MIN_MAX_BLOCK = 9000;
bool animationUploadOk = false;
bool animationUploadSeen = false;
}

static String formatUptime()
{
  unsigned long totalSeconds = millis() / 1000UL;
  unsigned long hours = totalSeconds / 3600UL;
  unsigned long minutes = (totalSeconds % 3600UL) / 60UL;
  unsigned long seconds = totalSeconds % 60UL;
  return String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
}

static int clampIntRange(int value, int minValue, int maxValue)
{
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

static bool assignCompactText(String& target, const char* value, unsigned int maxLength)
{
  if (!value) value = "";
  char buffer[49];
  unsigned int limit = maxLength < sizeof(buffer) - 1 ? maxLength : sizeof(buffer) - 1;
  unsigned int start = 0;
  while (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n') start++;
  unsigned int len = 0;
  while (len < limit && value[start + len] != '\0') len++;
  while (len > 0)
  {
    char c = value[start + len - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    len--;
  }
  memcpy(buffer, value + start, len);
  buffer[len] = '\0';
  if (target == buffer) return false;
  target = buffer;
  return true;
}

static float sanitizeFrametime(float value)
{
  if (isnan(value) || isinf(value) || value < 0.0f || value > 999.9f) return -1.0f;
  return value;
}

static String jsonEscape(String value)
{
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\r", " ");
  value.replace("\n", " ");
  return value;
}

ESP8266WebServer& webServerGet()
{
  return server;
}

static void handleRoot()
{
  resetMarkerCheckpoint("route_root");
  if (appState.isApMode)
  {
    String page = FPSTR(WIFI_SETUP_HTML);
    page.replace("%NETWORKS%", appState.scannedNetworks);
    server.send(200, "text/html", page);
    return;
  }
  String page = FPSTR(HOME_HTML);
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%SSID%", WiFi.SSID());
  page.replace("%RSSI%", String(WiFi.RSSI()));
  page.replace("%THEME%", themeManagerGetKey(themeManagerGetActive()));
  page.replace("%UPTIME%", formatUptime());
  page.replace("%RESET_REASON%", ESP.getResetReason());
  page.replace("%RESET_INFO%", ESP.getResetInfo());
  page.replace("%RESTART_INTENT%", appState.lastRestartIntent);
  page.replace("%LAST_CHECKPOINT%", appState.lastResetCheckpoint);
  page.replace("%HEAP%", String(ESP.getFreeHeap()));
  page.replace("%HEAP_FRAGMENTATION%", String(ESP.getHeapFragmentation()) + "%");
  page.replace("%MAX_FREE_BLOCK%", String(ESP.getMaxFreeBlockSize()));
  server.send(200, "text/html", page);
}

static void handleScan()
{
  resetMarkerCheckpoint("route_scan");
  wifiScanNetworks();
  handleRoot();
}

static void handleConnect()
{
  resetMarkerCheckpoint("route_connect");
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim(); pass.trim();
  if (ssid.length() == 0) { server.send(400, "text/plain", "SSID vazio."); return; }
  wifiSaveCredentials(ssid, pass);
  server.send(200, "text/html", "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;background:#111;color:#eee;text-align:center;padding:40px}h2{color:#ff9800}</style></head><body><h2>Wi-Fi salvo.</h2><p>O display vai reiniciar e tentar conectar.</p></body></html>");
  resetMarkerSet("wifi_connect");
  delay(1200); ESP.restart();
}

static void handleResetWifi()
{
  resetMarkerCheckpoint("route_reset_wifi");
  wifiClearCredentials();
  server.send(200, "text/html", "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;background:#111;color:#eee;text-align:center;padding:40px}h2{color:#ff9800}</style></head><body><h2>Wi-Fi apagado.</h2><p>O display vai reiniciar em modo setup.</p></body></html>");
  resetMarkerSet("reset_wifi");
  delay(1200); ESP.restart();
}

static void handleMetrics()
{
  resetMarkerCheckpoint("route_metrics");
  if (server.method() != HTTP_POST) { server.send(405, "application/json", "{\"ok\":false,\"error\":\"method_not_allowed\"}"); return; }
  if (ESP.getFreeHeap() < METRICS_MIN_FREE_HEAP || ESP.getMaxFreeBlockSize() < METRICS_MIN_MAX_BLOCK)
  {
    resetMarkerCheckpoint("route_metrics_heap_skip");
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"low_heap\"}");
    resetMarkerCheckpoint("route_metrics_heap_skip_done");
    return;
  }
  resetMarkerCheckpoint("route_metrics_body");
  String body = server.arg("plain");
  if (body.length() == 0) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty_body\"}"); return; }
  if (body.length() > 384) { server.send(413, "application/json", "{\"ok\":false,\"error\":\"payload_too_large\"}"); return; }
  // O payload Gamer HUD acrescenta strings e campos opcionais ao payload legado.
  resetMarkerCheckpoint("route_metrics_parse");
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body)) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}"); return; }
  if (!doc.containsKey("cpu") || !doc.containsKey("ram") || !doc.containsKey("gpu")) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_fields\"}"); return; }
  resetMarkerCheckpoint("route_metrics_apply");
  int cpu = metricsClampPercentInt(doc["cpu"] | 0);
  int ram = metricsClampPercentInt(doc["ram"] | 0);
  int gpu = metricsClampPercentInt(doc["gpu"] | 0);
  bool gamerChanged = cpu != appState.cpuCurrent || ram != appState.ramCurrent || gpu != appState.gpuCurrent;
  appState.cpuTarget = cpu; appState.ramTarget = ram; appState.gpuTarget = gpu;
  appState.cpuCurrent = cpu; appState.ramCurrent = ram; appState.gpuCurrent = gpu;
  if (doc.containsKey("disk"))
  {
    appState.diskCurrent = metricsClampPercentInt(doc["disk"] | DISK_MOCK_USAGE);
    appState.lastDiskDrawn = -1;
  }
  if (doc.containsKey("diskLabel"))
  {
    const char* diskLabel = doc["diskLabel"] | "";
    String previousDiskLabel = appState.diskLabel;
    if (assignCompactText(appState.diskLabel, diskLabel, 4) && appState.diskLabel.length() > 0)
    {
      appState.lastDiskLabelDrawn = "";
      gamerChanged = true;
    }
    else if (appState.diskLabel.length() == 0) appState.diskLabel = previousDiskLabel;
  }
  // Campos opcionais, ja normalizados pelo futuro PC Agent, para o Gamer HUD.
  // A rota /metrics e o contrato do PC Monitor permanecem os mesmos.
  if (doc.containsKey("game")) gamerChanged = assignCompactText(appState.gamerGame, doc["game"] | "", 48) || gamerChanged;
  if (doc.containsKey("source")) gamerChanged = assignCompactText(appState.gamerSource, doc["source"] | "", 12) || gamerChanged;
  if (doc.containsKey("fps"))
  {
    int fps = doc["fps"].isNull() ? -1 : clampIntRange(doc["fps"].as<int>(), 0, 999);
    if (appState.gamerFps != fps) { appState.gamerFps = fps; gamerChanged = true; }
  }
  if (doc.containsKey("frametime"))
  {
    float frametime = doc["frametime"].isNull() ? -1.0f : sanitizeFrametime(doc["frametime"].as<float>());
    if (appState.gamerFrametime != frametime) { appState.gamerFrametime = frametime; gamerChanged = true; }
  }
  if (doc.containsKey("gpuTemp"))
  {
    int gpuTemp = doc["gpuTemp"].isNull() ? -1 : clampIntRange(doc["gpuTemp"].as<int>(), 0, 120);
    if (appState.gamerGpuTemp != gpuTemp) { appState.gamerGpuTemp = gpuTemp; gamerChanged = true; }
  }
  if (doc.containsKey("cpuTemp"))
  {
    int cpuTemp = doc["cpuTemp"].isNull() ? -1 : clampIntRange(doc["cpuTemp"].as<int>(), 0, 120);
    if (appState.gamerCpuTemp != cpuTemp) { appState.gamerCpuTemp = cpuTemp; gamerChanged = true; }
  }
  if (doc.containsKey("vram"))
  {
    float vram = doc["vram"].as<float>();
    vram = doc["vram"].isNull() || isnan(vram) || isinf(vram) || vram < 0.0f || vram > 128.0f ? -1.0f : vram;
    if (appState.gamerVram != vram) { appState.gamerVram = vram; gamerChanged = true; }
  }
  if (gamerChanged) appState.gamerDataVersion++;
  appState.lastCpuDrawn = -1; appState.lastRamDrawn = -1; appState.lastGpuDrawn = -1;
  appState.lastPcMetricsReceived = millis();
  server.send(200, "application/json", "{\"ok\":true}");
  resetMarkerCheckpoint("route_metrics_done");
}

static void handleTheme()
{
  resetMarkerCheckpoint("route_theme");
  if (server.method() != HTTP_POST) { server.send(405, "application/json", "{\"ok\":false,\"error\":\"method_not_allowed\"}"); return; }
  String body = server.arg("plain");
  StaticJsonDocument<96> doc;
  if (body.length() == 0 || deserializeJson(doc, body)) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}"); return; }

  ThemeId theme;
  String themeKey = doc["theme"] | "";
  if (!themeManagerThemeFromKey(themeKey, theme)) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_theme\"}"); return; }
  if (!themeManagerSetActive(theme)) { server.send(500, "application/json", "{\"ok\":false,\"error\":\"theme_save_failed\"}"); return; }

  themeForceFullRedraw();
  String json = "{\"ok\":true,\"theme\":\"";
  json += themeManagerGetKey(themeManagerGetActive());
  json += "\"}";
  server.send(200, "application/json", json);
  resetMarkerCheckpoint("route_theme_done");
}

static void handleAnimationImage()
{
  resetMarkerCheckpoint("route_animation_image_done");
  if (!animationUploadSeen)
  {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_file\"}");
    return;
  }

  animationUploadSeen = false;
  if (!animationUploadOk)
  {
    String json = "{\"ok\":false,\"error\":\"";
    json += animationImageUploadError();
    json += "\"}";
    server.send(400, "application/json", json);
    return;
  }

  if (themeManagerGetActive() == THEME_ANIMATION) themeForceFullRedraw();

  String json = "{\"ok\":true,\"format\":\"rgb565_raw\",\"width\":";
  json += String(ANIMATION_IMAGE_WIDTH);
  json += ",\"height\":";
  json += String(ANIMATION_IMAGE_HEIGHT);
  json += ",\"bytes\":";
  json += String(animationImagePayloadBytes());
  json += "}";
  server.send(200, "application/json", json);
}

static void handleAnimationImageUpload()
{
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    resetMarkerCheckpoint("route_animation_image_start");
    animationUploadSeen = true;
    animationUploadOk = animationImageUploadBegin();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (animationUploadOk)
    {
      animationUploadOk = animationImageUploadWrite(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (animationUploadOk)
    {
      animationUploadOk = animationImageUploadEnd();
    }
    resetMarkerCheckpoint(animationUploadOk ? "route_animation_image_ok" : "route_animation_image_failed");
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  {
    animationImageUploadAbort();
    animationUploadOk = false;
    resetMarkerCheckpoint("route_animation_image_aborted");
  }
}

static void handleConfig()
{
  resetMarkerCheckpoint("route_config");
  String json = "{\"ok\":true,\"theme\":\"";
  json += themeManagerGetKey(themeManagerGetActive());
  json += "\",\"pcOnline\":";
  json += metricsHasRecentPcMetrics() ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
}

static void handleStatus()
{
  resetMarkerCheckpoint("route_status");
  if (ESP.getFreeHeap() < STATUS_MIN_FREE_HEAP || ESP.getMaxFreeBlockSize() < STATUS_MIN_MAX_BLOCK)
  {
    resetMarkerCheckpoint("route_status_low_heap");
    char json[256];
    snprintf(
      json,
      sizeof(json),
      "{\"name\":\"TinyDash\",\"mode\":\"%s\",\"theme\":\"%s\",\"uptimeMs\":%lu,\"pcOnline\":%s,\"heap\":%u,\"heapFragmentation\":%u,\"maxFreeBlockSize\":%u,\"lowHeap\":true}",
      appState.isApMode ? "AP" : "STA",
      themeManagerGetKey(themeManagerGetActive()),
      millis(),
      metricsHasRecentPcMetrics() ? "true" : "false",
      ESP.getFreeHeap(),
      ESP.getHeapFragmentation(),
      ESP.getMaxFreeBlockSize());
    server.send(200, "application/json", json);
    resetMarkerCheckpoint("route_status_low_heap_done");
    return;
  }
  resetMarkerCheckpoint("route_status_full");
  String ip = appState.isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String ssid = appState.isApMode ? String(AP_SSID) : WiFi.SSID();
  String json = "{";
  json += "\"name\":\"TinyDash\",";
  json += "\"mode\":\""; json += appState.isApMode ? "AP" : "STA"; json += "\",";
  json += "\"ip\":\""; json += ip; json += "\",";
  json += "\"ssid\":\""; json += ssid; json += "\",";
  json += "\"rssi\":";
  json += appState.isApMode ? "null" : String(WiFi.RSSI());
  json += ",";
  json += "\"theme\":\""; json += themeManagerGetKey(themeManagerGetActive()); json += "\",";
  json += "\"uptimeMs\":"; json += String(millis()); json += ",";
  json += "\"resetReason\":\""; json += jsonEscape(ESP.getResetReason()); json += "\",";
  json += "\"resetInfo\":\""; json += jsonEscape(ESP.getResetInfo()); json += "\",";
  json += "\"restartIntent\":\""; json += jsonEscape(appState.lastRestartIntent); json += "\",";
  json += "\"lastCheckpoint\":\""; json += jsonEscape(appState.lastResetCheckpoint); json += "\",";
  json += "\"cpu\":"; json += String(appState.cpuCurrent); json += ",";
  json += "\"ram\":"; json += String(appState.ramCurrent); json += ",";
  json += "\"gpu\":"; json += String(appState.gpuCurrent); json += ",";
  json += "\"disk\":"; json += String(appState.diskCurrent); json += ",";
  json += "\"diskLabel\":\""; json += appState.diskLabel; json += "\",";
  json += "\"pcOnline\":"; json += metricsHasRecentPcMetrics() ? "true" : "false"; json += ",";
  json += "\"lastPcMetricsAgeMs\":"; json += appState.lastPcMetricsReceived == 0 ? "null" : String(millis() - appState.lastPcMetricsReceived); json += ",";
  json += "\"temperature\":"; json += appState.hasWeather ? String(appState.weatherTemp, 1) : "null"; json += ",";
  json += "\"weather\":\""; json += appState.weatherText; json += "\",";
  json += "\"weatherStatus\":\""; json += jsonEscape(appState.weatherStatus); json += "\",";
  json += "\"heap\":"; json += String(ESP.getFreeHeap()); json += ",";
  json += "\"heapFragmentation\":"; json += String(ESP.getHeapFragmentation()); json += ",";
  json += "\"maxFreeBlockSize\":"; json += String(ESP.getMaxFreeBlockSize()); json += ",";
  json += "\"flashSize\":"; json += String(ESP.getFlashChipRealSize()); json += "}";
  json.setCharAt(json.length() - 1, ',');
  json += "\"animationImage\":";
  json += animationImageIsAvailable() ? "true" : "false";
  json += ",\"animationImageMaxBytes\":";
  json += String(animationImagePayloadBytes());
  json += ",\"animationImageStorageBytes\":";
  json += String(animationImageStorageBytes());
  json += "}";
  server.send(200, "application/json", json);
  resetMarkerCheckpoint("route_status_done");
}

void webServerSetup()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/reset-wifi", HTTP_GET, handleResetWifi);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/metrics", HTTP_POST, handleMetrics);
  server.on("/theme", HTTP_POST, handleTheme);
  server.on("/animation/image", HTTP_POST, handleAnimationImage, handleAnimationImageUpload);
  server.on("/config", HTTP_GET, handleConfig);
  otaUpdateSetup();
  server.begin();
}

void webServerHandleClient()
{
  server.handleClient();
}
