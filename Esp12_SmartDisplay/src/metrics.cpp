#include "metrics.h"

#include <TFT_eSPI.h>

#include "app_state.h"
#include "config.h"
#include "display_ui.h"

bool metricsHasRecentPcMetrics()
{
  return appState.lastPcMetricsReceived != 0 && millis() - appState.lastPcMetricsReceived <= PC_METRICS_TIMEOUT_MS;
}

int metricsClampPercentInt(int value)
{
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

static int randomTargetNear(int current, int minValue, int maxValue, int maxDelta)
{
  int target = current + random(-maxDelta, maxDelta + 1);
  if (target < minValue) target = minValue;
  if (target > maxValue) target = maxValue;
  return target;
}

static bool moveTowards(int& current, int target, int step)
{
  if (current == target) return false;
  if (current < target) { current += step; if (current > target) current = target; }
  else { current -= step; if (current < target) current = target; }
  return true;
}

void metricsUpdateFakeTargets()
{
  unsigned long now = millis();
  if (now - appState.lastCpuTargetUpdate >= CPU_TARGET_INTERVAL_MS) { appState.lastCpuTargetUpdate = now; appState.cpuTarget = randomTargetNear(appState.cpuCurrent, 5, 96, 28); }
  if (now - appState.lastRamTargetUpdate >= RAM_TARGET_INTERVAL_MS) { appState.lastRamTargetUpdate = now; appState.ramTarget = randomTargetNear(appState.ramCurrent, 35, 90, 10); }
  if (now - appState.lastGpuTargetUpdate >= GPU_TARGET_INTERVAL_MS) { appState.lastGpuTargetUpdate = now; appState.gpuTarget = randomTargetNear(appState.gpuCurrent, 0, 98, 35); }
}

void metricsAnimateFakeValues()
{
  unsigned long now = millis();
  if (now - appState.lastAnimationUpdate < ANIMATION_INTERVAL_MS) return;
  appState.lastAnimationUpdate = now;
  if (metricsHasRecentPcMetrics())
  {
    moveTowards(appState.cpuCurrent, appState.cpuTarget, 10); moveTowards(appState.ramCurrent, appState.ramTarget, 6); moveTowards(appState.gpuCurrent, appState.gpuTarget, 12);
  }
  else
  {
    moveTowards(appState.cpuCurrent, appState.cpuTarget, 2); moveTowards(appState.ramCurrent, appState.ramTarget, 1); moveTowards(appState.gpuCurrent, appState.gpuTarget, 3);
  }
}

void metricsUpdateDisplayIfNeeded()
{
  if (appState.cpuCurrent != appState.lastCpuDrawn) { displayUiDrawMetricRow(CPU_Y, "CPU", appState.cpuCurrent, TFT_BLUE); appState.lastCpuDrawn = appState.cpuCurrent; }
  if (appState.ramCurrent != appState.lastRamDrawn) { displayUiDrawMetricRow(RAM_Y, "RAM", appState.ramCurrent, TFT_GREEN); appState.lastRamDrawn = appState.ramCurrent; }
  if (appState.gpuCurrent != appState.lastGpuDrawn) { displayUiDrawMetricRow(GPU_Y, "GPU", appState.gpuCurrent, TFT_CYAN); appState.lastGpuDrawn = appState.gpuCurrent; }
}
