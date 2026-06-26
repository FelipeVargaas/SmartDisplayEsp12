#pragma once

#include <Arduino.h>

void resetMarkerSet(const char* reason);
String resetMarkerConsume();
void resetMarkerCheckpoint(const char* checkpoint);
String resetMarkerReadCheckpoint();
