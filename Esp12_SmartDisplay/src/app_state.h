#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

struct WeatherForecastDay
{
  int high;
  int low;
  int code;
  bool valid;
};

struct AppState
{
  TFT_eSPI tft;
  TFT_eSprite metricSprite;
  bool metricSpriteReady;
  String scannedNetworks;
  bool isApMode;
  bool safeMode;
  String lastRestartIntent;
  String lastResetCheckpoint;
  bool otaMaintenanceMode;
  unsigned long otaMaintenanceUntil;
  unsigned long otaMaintenanceLastFrame;
  uint8_t otaMaintenanceFrame;

  int cpuCurrent;
  int ramCurrent;
  int gpuCurrent;
  int diskCurrent;
  String diskLabel;
  int cpuTarget;
  int ramTarget;
  int gpuTarget;
  unsigned long lastPcMetricsReceived;
  int lastCpuDrawn;
  int lastRamDrawn;
  int lastGpuDrawn;
  int lastDiskDrawn;
  String lastDiskLabelDrawn;

  // Dados normalizados para o tema Gamer HUD. Fontes customizadas podem ser
  // introduzidas futuramente apenas na camada de renderizacao do tema.
  String gamerGame;
  String gamerSource;
  int gamerFps;
  float gamerFrametime;
  int gamerGpuTemp;
  int gamerCpuTemp;
  float gamerVram;
  uint32_t gamerDataVersion;
  uint32_t gamerDrawnVersion;
  unsigned long gamerLastStatusUpdate;
  bool gamerShowSource;
  bool gamerLastPcOnline;

  String lastTimeDrawn;
  String lastWeatherDrawn;
  String lastTopLabelDrawn;
  float weatherTemp;
  int weatherTodayLow;
  int weatherHumidity;
  int weatherCode;
  bool hasWeather;
  String weatherText;
  String weatherStatus;
  String weatherUpdatedAt;
  WeatherForecastDay weatherForecast[2];
  uint8_t weatherRetryCount;
  bool lastThemeUsesWeather;

  unsigned long lastCpuTargetUpdate;
  unsigned long lastRamTargetUpdate;
  unsigned long lastGpuTargetUpdate;
  unsigned long lastAnimationUpdate;
  unsigned long lastClockCheck;
  unsigned long lastWeatherUpdate;
  unsigned long lastWeatherSuccess;
  unsigned long nextWeatherUpdate;
  unsigned long lastFooterUpdate;
  unsigned long lastFooterStatusUpdate;
  unsigned long lastTopLabelUpdate;
  uint8_t topLabelIndex;
  uint8_t footerStatusIndex;

  AppState();
};

extern AppState appState;
