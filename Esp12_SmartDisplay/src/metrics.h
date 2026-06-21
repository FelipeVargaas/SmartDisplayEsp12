#pragma once

#include <Arduino.h>

bool metricsHasRecentPcMetrics();
int metricsClampPercentInt(int value);
void metricsUpdateFakeTargets();
void metricsAnimateFakeValues();
void metricsUpdateDisplayIfNeeded();
