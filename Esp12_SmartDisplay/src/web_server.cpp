#include "web_server.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "app_state.h"
#include "config.h"
#include "metrics.h"
#include "ota_update.h"
#include "web_pages.h"
#include "wifi_manager.h"

static ESP8266WebServer server(80);

ESP8266WebServer& webServerGet()
{
  return server;
}

static void handleRoot()
{
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
  server.send(200, "text/html", page);
}

static void handleScan()
{
  wifiScanNetworks();
  handleRoot();
}

static void handleConnect()
{
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim(); pass.trim();
  if (ssid.length() == 0) { server.send(400, "text/plain", "SSID vazio."); return; }
  wifiSaveCredentials(ssid, pass);
  server.send(200, "text/html", "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;background:#111;color:#eee;text-align:center;padding:40px}h2{color:#ff9800}</style></head><body><h2>Wi-Fi salvo.</h2><p>O display vai reiniciar e tentar conectar.</p></body></html>");
  delay(1200); ESP.restart();
}

static void handleResetWifi()
{
  wifiClearCredentials();
  server.send(200, "text/html", "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;background:#111;color:#eee;text-align:center;padding:40px}h2{color:#ff9800}</style></head><body><h2>Wi-Fi apagado.</h2><p>O display vai reiniciar em modo setup.</p></body></html>");
  delay(1200); ESP.restart();
}

static void handleMetrics()
{
  if (server.method() != HTTP_POST) { server.send(405, "application/json", "{\"ok\":false,\"error\":\"method_not_allowed\"}"); return; }
  String body = server.arg("plain");
  if (body.length() == 0) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty_body\"}"); return; }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}"); return; }
  if (!doc.containsKey("cpu") || !doc.containsKey("ram") || !doc.containsKey("gpu")) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_fields\"}"); return; }
  int cpu = metricsClampPercentInt(doc["cpu"] | 0);
  int ram = metricsClampPercentInt(doc["ram"] | 0);
  int gpu = metricsClampPercentInt(doc["gpu"] | 0);
  appState.cpuTarget = cpu; appState.ramTarget = ram; appState.gpuTarget = gpu;
  appState.cpuCurrent = cpu; appState.ramCurrent = ram; appState.gpuCurrent = gpu;
  if (doc.containsKey("disk"))
  {
    appState.diskCurrent = metricsClampPercentInt(doc["disk"] | DISK_MOCK_USAGE);
    appState.lastDiskDrawn = -1;
  }
  if (doc.containsKey("diskLabel"))
  {
    String diskLabel = doc["diskLabel"].as<String>();
    diskLabel.trim();
    if (diskLabel.length() > 0)
    {
      if (diskLabel.length() > 4) diskLabel = diskLabel.substring(0, 4);
      appState.diskLabel = diskLabel;
      appState.lastDiskLabelDrawn = "";
    }
  }
  appState.lastCpuDrawn = -1; appState.lastRamDrawn = -1; appState.lastGpuDrawn = -1;
  appState.lastPcMetricsReceived = millis();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleStatus()
{
  String ip = appState.isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String ssid = appState.isApMode ? String(AP_SSID) : WiFi.SSID();
  String json = "{";
  json += "\"name\":\"PC Monitor\",";
  json += "\"mode\":\""; json += appState.isApMode ? "AP" : "STA"; json += "\",";
  json += "\"ip\":\""; json += ip; json += "\",";
  json += "\"ssid\":\""; json += ssid; json += "\",";
  json += "\"cpu\":"; json += String(appState.cpuCurrent); json += ",";
  json += "\"ram\":"; json += String(appState.ramCurrent); json += ",";
  json += "\"gpu\":"; json += String(appState.gpuCurrent); json += ",";
  json += "\"disk\":"; json += String(appState.diskCurrent); json += ",";
  json += "\"diskLabel\":\""; json += appState.diskLabel; json += "\",";
  json += "\"pcOnline\":"; json += metricsHasRecentPcMetrics() ? "true" : "false"; json += ",";
  json += "\"lastPcMetricsAgeMs\":"; json += appState.lastPcMetricsReceived == 0 ? "null" : String(millis() - appState.lastPcMetricsReceived); json += ",";
  json += "\"temperature\":"; json += appState.hasWeather ? String(appState.weatherTemp, 1) : "null"; json += ",";
  json += "\"weather\":\""; json += appState.weatherText; json += "\",";
  json += "\"heap\":"; json += String(ESP.getFreeHeap()); json += ",";
  json += "\"flashSize\":"; json += String(ESP.getFlashChipRealSize()); json += "}";
  server.send(200, "application/json", json);
}

void webServerSetup()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/reset-wifi", HTTP_GET, handleResetWifi);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/metrics", HTTP_POST, handleMetrics);
  otaUpdateSetup();
  server.begin();
}

void webServerHandleClient()
{
  server.handleClient();
}
