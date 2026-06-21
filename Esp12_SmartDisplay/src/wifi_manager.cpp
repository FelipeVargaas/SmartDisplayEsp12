#include "wifi_manager.h"

#include <EEPROM.h>
#include <ESP8266WiFi.h>

#include "app_state.h"
#include "config.h"
#include "display_ui.h"

void wifiSaveCredentials(const String& ssid, const String& pass)
{
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_SSID; i++) EEPROM.write(SSID_ADDR + i, i < (int)ssid.length() ? ssid[i] : 0);
  for (int i = 0; i < MAX_PASS; i++) EEPROM.write(PASS_ADDR + i, i < (int)pass.length() ? pass[i] : 0);
  EEPROM.commit(); EEPROM.end();
}

bool wifiLoadCredentials(String& ssid, String& pass)
{
  EEPROM.begin(EEPROM_SIZE);
  char ssidBuf[MAX_SSID + 1]; char passBuf[MAX_PASS + 1];
  for (int i = 0; i < MAX_SSID; i++) ssidBuf[i] = EEPROM.read(SSID_ADDR + i);
  ssidBuf[MAX_SSID] = 0;
  for (int i = 0; i < MAX_PASS; i++) passBuf[i] = EEPROM.read(PASS_ADDR + i);
  passBuf[MAX_PASS] = 0;
  EEPROM.end();
  ssid = String(ssidBuf); pass = String(passBuf); ssid.trim(); pass.trim();
  if (ssid.length() == 0) return false;
  return (uint8_t)ssid[0] != 0xFF;
}

void wifiClearCredentials()
{
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
  EEPROM.commit(); EEPROM.end();
}

static bool tryConnectWiFi(const String& ssid, const String& pass)
{
  displayUiDrawConnectingNetwork(ssid);
  WiFi.mode(WIFI_STA); WiFi.begin(ssid.c_str(), pass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { delay(500); attempts++; yield(); }
  return WiFi.status() == WL_CONNECTED;
}

bool wifiConnect()
{
  String ssid; String pass;
  if (!wifiLoadCredentials(ssid, pass)) { ssid = DEFAULT_WIFI_SSID; pass = DEFAULT_WIFI_PASS; }
  if (ssid.length() == 0) return false;
  if (tryConnectWiFi(ssid, pass)) { appState.isApMode = false; return true; }
  displayUiDrawConnectionFailedScreen();
  return false;
}

static String htmlEscape(String value)
{
  value.replace("&", "&amp;"); value.replace("<", "&lt;"); value.replace(">", "&gt;");
  value.replace("\"", "&quot;"); value.replace("'", "&#39;");
  return value;
}

void wifiScanNetworks()
{
  appState.scannedNetworks = "";
  int n = WiFi.scanNetworks();
  if (n <= 0) { appState.scannedNetworks = "<p class='msg'>Nenhuma rede encontrada.</p>"; return; }
  for (int i = 0; i < n; i++)
  {
    String safeSsid = htmlEscape(WiFi.SSID(i));
    appState.scannedNetworks += "<div class='net' onclick=\"sel('";
    appState.scannedNetworks += safeSsid;
    appState.scannedNetworks += "')\">";
    appState.scannedNetworks += safeSsid;
    appState.scannedNetworks += " ("; appState.scannedNetworks += String(WiFi.RSSI(i)); appState.scannedNetworks += " dBm)";
    if (WiFi.encryptionType(i) == ENC_TYPE_NONE) appState.scannedNetworks += " [aberta]";
    appState.scannedNetworks += "</div>";
  }
}

void wifiStartApMode()
{
  appState.isApMode = true;
  WiFi.mode(WIFI_AP_STA); WiFi.softAP(AP_SSID, AP_PASS);
  delay(300);
  wifiScanNetworks();
  displayUiDrawApScreen();
}
