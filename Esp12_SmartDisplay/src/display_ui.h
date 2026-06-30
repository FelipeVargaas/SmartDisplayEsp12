#pragma once

#include <Arduino.h>

void displayUiInit();
void displayUiInitSprites();
void displayUiDrawBootScreen();
void displayUiDrawStartupInfo(const String& ipAddress);
void displayUiDrawConnectingNetwork(const String& ssid);
void displayUiDrawApScreen();
void displayUiDrawConnectionFailedScreen();
void displayUiDrawSafeModeScreen(const String& ipAddress);
void displayUiDrawOtaMaintenanceScreen(const String& ipAddress);
void displayUiUpdateOtaMaintenanceProgress();
void displayUiDrawDashboardBase();
void displayUiUpdateHeaderIfNeeded();
void displayUiUpdateTopLabelIfNeeded();
void displayUiDrawFooter();
void displayUiDrawMetricRow(int y, const String& label, int value, uint16_t baseColor);
