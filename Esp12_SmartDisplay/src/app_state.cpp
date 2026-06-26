#include "app_state.h"

#include "config.h"

AppState::AppState() :
  tft(), metricSprite(&tft), metricSpriteReady(false), isApMode(false), safeMode(false), lastRestartIntent("unknown"), lastResetCheckpoint("unknown"),
  cpuCurrent(-1), ramCurrent(-1), gpuCurrent(-1), diskCurrent(-1), diskLabel("SSD"),
  cpuTarget(-1), ramTarget(-1), gpuTarget(-1), lastPcMetricsReceived(0),
  lastCpuDrawn(-1), lastRamDrawn(-1), lastGpuDrawn(-1), lastDiskDrawn(-1), lastDiskLabelDrawn(""),
  gamerGame(""), gamerSource(""), gamerFps(-1), gamerFrametime(-1.0f), gamerGpuTemp(-1), gamerCpuTemp(-1), gamerVram(-1.0f),
  gamerDataVersion(0), gamerDrawnVersion(UINT32_MAX), gamerLastStatusUpdate(0), gamerShowSource(true), gamerLastPcOnline(false),
  weatherTemp(0.0f), weatherCode(-1), hasWeather(false), weatherText("Clima"), weatherStatus("not_started"), weatherRetryCount(0), lastThemeUsesWeather(false),
  lastCpuTargetUpdate(0), lastRamTargetUpdate(0), lastGpuTargetUpdate(0),
  lastAnimationUpdate(0), lastClockCheck(0), lastWeatherUpdate(0), lastFooterUpdate(0), lastFooterStatusUpdate(0),
  lastTopLabelUpdate(0), topLabelIndex(0), footerStatusIndex(0)
{
}

AppState appState;
