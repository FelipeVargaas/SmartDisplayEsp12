#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

struct AppState
{
  TFT_eSPI tft;
  TFT_eSprite metricSprite;
  bool metricSpriteReady;
  String scannedNetworks;
  bool isApMode;

  int cpuCurrent;
  int ramCurrent;
  int gpuCurrent;
  int cpuTarget;
  int ramTarget;
  int gpuTarget;
  unsigned long lastPcMetricsReceived;
  int lastCpuDrawn;
  int lastRamDrawn;
  int lastGpuDrawn;

  String lastTimeDrawn;
  String lastWeatherDrawn;
  float weatherTemp;
  bool hasWeather;
  String weatherText;

  unsigned long lastCpuTargetUpdate;
  unsigned long lastRamTargetUpdate;
  unsigned long lastGpuTargetUpdate;
  unsigned long lastAnimationUpdate;
  unsigned long lastClockCheck;
  unsigned long lastWeatherUpdate;
  unsigned long lastFooterUpdate;

  AppState();
};

extern AppState appState;
