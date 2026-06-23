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
  bool safeMode;

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
  String lastTopLabelDrawn;
  float weatherTemp;
  int weatherCode;
  bool hasWeather;
  String weatherText;

  unsigned long lastCpuTargetUpdate;
  unsigned long lastRamTargetUpdate;
  unsigned long lastGpuTargetUpdate;
  unsigned long lastAnimationUpdate;
  unsigned long lastClockCheck;
  unsigned long lastWeatherUpdate;
  unsigned long lastFooterUpdate;
  unsigned long lastFooterStatusUpdate;
  unsigned long lastTopLabelUpdate;
  uint8_t topLabelIndex;
  uint8_t footerStatusIndex;

  AppState();
};

extern AppState appState;
