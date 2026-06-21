#pragma once

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_INVERT true
#define DISPLAY_BL_PIN 5
#define DISPLAY_BL_ON LOW

static const char* const DEFAULT_WIFI_SSID = "AP303_2G";
static const char* const DEFAULT_WIFI_PASS = "luiz2610";
static const char* const AP_SSID = "MiniScreen-Setup";
static const char* const AP_PASS = "12345678";
static const char* const OTA_USER = "admin";
static const char* const OTA_PASS = "vargas";

const float WEATHER_LAT = -22.9068;
const float WEATHER_LON = -43.1729;
static const char* const WEATHER_TIMEZONE = "America%2FSao_Paulo";
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;
const unsigned long WEATHER_UPDATE_INTERVAL_MS = 10UL * 60UL * 1000UL;

#define EEPROM_SIZE 96
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MAX_SSID 32
#define MAX_PASS 64

const unsigned long PC_METRICS_TIMEOUT_MS = 5000;
const bool USE_FAKE_METRICS_WHEN_PC_OFFLINE = false;
const unsigned long CPU_TARGET_INTERVAL_MS = 900;
const unsigned long RAM_TARGET_INTERVAL_MS = 1400;
const unsigned long GPU_TARGET_INTERVAL_MS = 1800;
const unsigned long ANIMATION_INTERVAL_MS = 40;
const unsigned long CLOCK_CHECK_INTERVAL_MS = 1000;
const unsigned long FOOTER_UPDATE_INTERVAL_MS = 3000;

const int HEADER_H = 98;
const int METRIC_X = 12;
const int METRIC_W = 216;
const int METRIC_H = 32;
const int CPU_Y = 105;
const int RAM_Y = 143;
const int GPU_Y = 181;
