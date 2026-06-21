#include "app_state.h"

AppState::AppState() :
  tft(), metricSprite(&tft), metricSpriteReady(false), isApMode(false),
  cpuCurrent(37), ramCurrent(68), gpuCurrent(22),
  cpuTarget(37), ramTarget(68), gpuTarget(22), lastPcMetricsReceived(0),
  lastCpuDrawn(-1), lastRamDrawn(-1), lastGpuDrawn(-1),
  weatherTemp(0.0f), hasWeather(false), weatherText("Clima"),
  lastCpuTargetUpdate(0), lastRamTargetUpdate(0), lastGpuTargetUpdate(0),
  lastAnimationUpdate(0), lastClockCheck(0), lastWeatherUpdate(0), lastFooterUpdate(0)
{
}

AppState appState;
