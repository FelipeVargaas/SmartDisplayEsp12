#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "config.h"

namespace
{
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer updater;

bool apMode = false;

bool loadCredentials(String& ssid, String& pass)
{
  EEPROM.begin(EEPROM_SIZE);

  char ssidBuffer[MAX_SSID + 1];
  char passBuffer[MAX_PASS + 1];

  for (int i = 0; i < MAX_SSID; i++) ssidBuffer[i] = EEPROM.read(SSID_ADDR + i);
  ssidBuffer[MAX_SSID] = '\0';

  for (int i = 0; i < MAX_PASS; i++) passBuffer[i] = EEPROM.read(PASS_ADDR + i);
  passBuffer[MAX_PASS] = '\0';

  EEPROM.end();

  if ((uint8_t)ssidBuffer[0] == 0xFF || ssidBuffer[0] == '\0') return false;

  ssid = String(ssidBuffer);
  pass = String(passBuffer);
  ssid.trim();
  pass.trim();

  return ssid.length() > 0;
}

void saveCredentials(const String& ssid, const String& pass)
{
  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; i < MAX_SSID; i++) EEPROM.write(SSID_ADDR + i, i < (int)ssid.length() ? ssid[i] : 0);
  for (int i = 0; i < MAX_PASS; i++) EEPROM.write(PASS_ADDR + i, i < (int)pass.length() ? pass[i] : 0);

  EEPROM.commit();
  EEPROM.end();
}

void clearCredentials()
{
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_SSID + MAX_PASS; i++) EEPROM.write(SSID_ADDR + i, 0);
  EEPROM.commit();
  EEPROM.end();
}

bool connectSta()
{
  String ssid;
  String pass;

  if (!loadCredentials(ssid, pass))
  {
    ssid = DEFAULT_WIFI_SSID;
    pass = DEFAULT_WIFI_PASS;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  for (int attempt = 0; attempt < 40; attempt++)
  {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(250);
    yield();
  }

  return WiFi.status() == WL_CONNECTED;
}

void startAp()
{
  apMode = true;
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
}

void sendHome()
{
  const String ip = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  const String mode = apMode ? "AP" : "STA";
  const String ssid = apMode ? String(AP_SSID) : WiFi.SSID();

  String page;
  page.reserve(1200);
  page += F("<!doctype html><html><head><meta charset='utf-8'>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>TinyDash Recovery OTA</title>");
  page += F("<style>body{margin:0;background:#071018;color:#eef;font-family:Arial,sans-serif;padding:28px}");
  page += F("h1{color:#ff7a1a}a,button{background:#111d29;color:#ff7a1a;border:1px solid #ff7a1a;border-radius:6px;padding:10px 14px;text-decoration:none;font-weight:700}");
  page += F("input{display:block;margin:8px 0 14px;padding:10px;width:260px;max-width:100%;background:#071018;color:#fff;border:1px solid #2d4358;border-radius:6px}");
  page += F(".box{max-width:560px}.muted{color:#9fc7e7}</style></head><body><div class='box'>");
  page += F("<h1>TinyDash Recovery OTA</h1>");
  page += F("<p class='muted'>Firmware minimo para recuperar atualizacao.</p>");
  page += F("<p>Modo: <b>");
  page += mode;
  page += F("</b><br>IP: <b>");
  page += ip;
  page += F("</b><br>SSID: <b>");
  page += ssid;
  page += F("</b><br>Heap: <b>");
  page += String(ESP.getFreeHeap());
  page += F("</b><br>Bloco max.: <b>");
  page += String(ESP.getMaxFreeBlockSize());
  page += F("</b></p><p><a href='/update'>Abrir OTA</a></p>");
  page += F("<h2>Wi-Fi</h2><form method='post' action='/connect'>");
  page += F("<input name='ssid' placeholder='SSID'><input name='pass' placeholder='Senha' type='password'>");
  page += F("<button type='submit'>Salvar e reiniciar</button></form>");
  page += F("<p><a href='/status'>Status JSON</a> <a href='/reset-wifi'>Reset Wi-Fi</a></p>");
  page += F("</div></body></html>");

  server.send(200, "text/html", page);
}

void sendStatus()
{
  const String ip = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  const String ssid = apMode ? String(AP_SSID) : WiFi.SSID();

  char json[384];
  snprintf(
    json,
    sizeof(json),
    "{\"name\":\"TinyDash Recovery\",\"version\":\"recovery-ota\",\"mode\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"rssi\":%d,\"uptimeMs\":%lu,\"heap\":%u,\"heapFragmentation\":%u,\"maxFreeBlockSize\":%u,\"flashSize\":%u}",
    apMode ? "AP" : "STA",
    ip.c_str(),
    ssid.c_str(),
    apMode ? 0 : WiFi.RSSI(),
    millis(),
    ESP.getFreeHeap(),
    ESP.getHeapFragmentation(),
    ESP.getMaxFreeBlockSize(),
    ESP.getFlashChipRealSize());

  server.send(200, "application/json", json);
}

void handleConnect()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "text/plain", "method_not_allowed");
    return;
  }

  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  pass.trim();

  if (ssid.length() == 0)
  {
    server.send(400, "text/plain", "SSID vazio.");
    return;
  }

  saveCredentials(ssid, pass);
  server.send(200, "text/html", "<html><body><h1>Wi-Fi salvo.</h1><p>Reiniciando...</p></body></html>");
  delay(800);
  ESP.restart();
}

void handleResetWifi()
{
  clearCredentials();
  server.send(200, "text/html", "<html><body><h1>Wi-Fi apagado.</h1><p>Reiniciando...</p></body></html>");
  delay(800);
  ESP.restart();
}

void handleNotFound()
{
  server.send(404, "text/plain", "not_found");
}
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("TinyDash Recovery OTA"));

  if (!connectSta()) startAp();

  server.on("/", HTTP_GET, sendHome);
  server.on("/status", HTTP_GET, sendStatus);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/reset-wifi", HTTP_GET, handleResetWifi);
  server.onNotFound(handleNotFound);

  updater.setup(&server, "/update", OTA_USER, OTA_PASS);
  server.begin();
}

void loop()
{
  server.handleClient();
  yield();
}
