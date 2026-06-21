#pragma once

#include <Arduino.h>

void wifiSaveCredentials(const String& ssid, const String& pass);
bool wifiLoadCredentials(String& ssid, String& pass);
void wifiClearCredentials();
bool wifiConnect();
void wifiStartApMode();
void wifiScanNetworks();
