#pragma once

#include <Arduino.h>

void displayUiInit();
void displayUiInitSprites();
void displayUiDrawBootScreen();
void displayUiDrawConnectingNetwork(const String& ssid);
void displayUiDrawApScreen();
void displayUiDrawConnectionFailedScreen();
void displayUiDrawDashboardBase();
void displayUiUpdateHeaderIfNeeded();
void displayUiDrawFooter();
void displayUiDrawMetricRow(int y, const String& label, int value, uint16_t baseColor);
