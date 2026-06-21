#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

// ======================================================
// CONFIGURAÇÕES PRINCIPAIS
// ======================================================

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240

// Corrige display ST7789 que estava com fundo branco e cores complementares.
#define DISPLAY_INVERT  true

// Backlight
#define DISPLAY_BL_PIN  5
#define DISPLAY_BL_ON   LOW

// Rede padrão inicial.
// Depois podemos remover isso e deixar só pelo setup web.
const char* DEFAULT_WIFI_SSID = "AP303_2G";
const char* DEFAULT_WIFI_PASS = "luiz2610";

// AP de configuração, caso não consiga conectar no Wi-Fi.
const char* AP_SSID = "MiniScreen-Setup";
const char* AP_PASS = "12345678";

// OTA
const char* OTA_USER = "admin";
const char* OTA_PASS = "vargas";

// ======================================================
// LOCALIZAÇÃO / HORA / CLIMA
// ======================================================

// Rio de Janeiro/RJ
const float WEATHER_LAT = -22.9068;
const float WEATHER_LON = -43.1729;

const char* WEATHER_TIMEZONE = "America%2FSao_Paulo";

// UTC-3 Brasil
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

const unsigned long WEATHER_UPDATE_INTERVAL_MS = 10UL * 60UL * 1000UL;

// ======================================================
// EEPROM - Wi-Fi salvo
// ======================================================

#define EEPROM_SIZE 96
#define SSID_ADDR   0
#define PASS_ADDR   32
#define MAX_SSID    32
#define MAX_PASS    64

// ======================================================
// OBJETOS GLOBAIS
// ======================================================

TFT_eSPI tft = TFT_eSPI();

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// Sprite pequeno reutilizado para as linhas CPU/RAM/GPU.
// Não usar sprite tela cheia no ESP8266. Ele chora em heap.
TFT_eSprite metricSprite = TFT_eSprite(&tft);

bool metricSpriteReady = false;

String scannedNetworks = "";
bool isApMode = false;

// ======================================================
// DASHBOARD / ESTADO
// ======================================================

int cpuCurrent = 37;
int ramCurrent = 68;
int gpuCurrent = 22;

int cpuTarget = 37;
int ramTarget = 68;
int gpuTarget = 22;

unsigned long lastPcMetricsReceived = 0;

const unsigned long PC_METRICS_TIMEOUT_MS = 5000;

// Para POC, melhor false.
// Assim, quando o PC parar de enviar, o display não inventa número fake.
const bool USE_FAKE_METRICS_WHEN_PC_OFFLINE = false;

int lastCpuDrawn = -1;
int lastRamDrawn = -1;
int lastGpuDrawn = -1;

String lastTimeDrawn = "";
String lastWeatherDrawn = "";

float weatherTemp = 0.0f;
bool hasWeather = false;
String weatherText = "Clima";

unsigned long lastCpuTargetUpdate = 0;
unsigned long lastRamTargetUpdate = 0;
unsigned long lastGpuTargetUpdate = 0;
unsigned long lastAnimationUpdate = 0;
unsigned long lastClockCheck = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastFooterUpdate = 0;

const unsigned long CPU_TARGET_INTERVAL_MS = 900;
const unsigned long RAM_TARGET_INTERVAL_MS = 1400;
const unsigned long GPU_TARGET_INTERVAL_MS = 1800;

const unsigned long ANIMATION_INTERVAL_MS = 40;
const unsigned long CLOCK_CHECK_INTERVAL_MS = 1000;
const unsigned long FOOTER_UPDATE_INTERVAL_MS = 3000;

// Layout
const int HEADER_H = 98;

const int METRIC_X = 12;
const int METRIC_W = 216;
const int METRIC_H = 32;

const int CPU_Y = 105;
const int RAM_Y = 143;
const int GPU_Y = 181;

// ======================================================
// EEPROM HELPERS
// ======================================================

void saveCredentials(const String& ssid, const String& pass)
{
  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; i < MAX_SSID; i++)
  {
    EEPROM.write(SSID_ADDR + i, i < (int)ssid.length() ? ssid[i] : 0);
  }

  for (int i = 0; i < MAX_PASS; i++)
  {
    EEPROM.write(PASS_ADDR + i, i < (int)pass.length() ? pass[i] : 0);
  }

  EEPROM.commit();
  EEPROM.end();
}

bool loadCredentials(String& ssid, String& pass)
{
  EEPROM.begin(EEPROM_SIZE);

  char ssidBuf[MAX_SSID + 1];
  char passBuf[MAX_PASS + 1];

  for (int i = 0; i < MAX_SSID; i++)
  {
    ssidBuf[i] = EEPROM.read(SSID_ADDR + i);
  }
  ssidBuf[MAX_SSID] = 0;

  for (int i = 0; i < MAX_PASS; i++)
  {
    passBuf[i] = EEPROM.read(PASS_ADDR + i);
  }
  passBuf[MAX_PASS] = 0;

  EEPROM.end();

  ssid = String(ssidBuf);
  pass = String(passBuf);

  ssid.trim();
  pass.trim();

  if (ssid.length() == 0)
  {
    return false;
  }

  if ((uint8_t)ssid[0] == 0xFF)
  {
    return false;
  }

  return true;
}

void clearCredentials()
{
  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; i < EEPROM_SIZE; i++)
  {
    EEPROM.write(i, 0);
  }

  EEPROM.commit();
  EEPROM.end();
}

// ======================================================
// DISPLAY BASE
// ======================================================

void initDisplay()
{
  pinMode(DISPLAY_BL_PIN, OUTPUT);
  digitalWrite(DISPLAY_BL_PIN, DISPLAY_BL_ON);

  delay(100);

  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(DISPLAY_INVERT);
  tft.fillScreen(TFT_BLACK);
}

void initSprites()
{
  metricSprite.setColorDepth(8);

  void* result = metricSprite.createSprite(METRIC_W, METRIC_H);

  metricSpriteReady = result != nullptr;
}

void displayCentered(const String& text, int y, int size, uint16_t color)
{
  tft.setTextSize(size);
  tft.setTextColor(color, TFT_BLACK);

  int16_t textWidth = tft.textWidth(text);
  int16_t x = (DISPLAY_WIDTH - textWidth) / 2;

  if (x < 0)
  {
    x = 0;
  }

  tft.setCursor(x, y);
  tft.print(text);
}

void drawBootScreen()
{
  tft.fillScreen(TFT_BLACK);

  // Sem acento no texto do TFT para evitar bagunça de charset.
  displayCentered("Ola Vargas", 68, 3, TFT_ORANGE);
  displayCentered("Conectando...", 125, 2, TFT_WHITE);
}

void drawConnectingNetwork(const String& ssid)
{
  tft.fillRect(0, 150, DISPLAY_WIDTH, 40, TFT_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);

  String text = "Rede: " + ssid;
  int w = tft.textWidth(text);
  tft.setCursor((DISPLAY_WIDTH - w) / 2, 158);
  tft.print(text);
}

void drawApScreen()
{
  tft.fillScreen(TFT_BLACK);

  displayCentered("Wi-Fi Setup", 12, 2, TFT_YELLOW);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setCursor(10, 47);
  tft.print("Conecte nesta rede:");

  tft.setCursor(10, 70);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print(AP_SSID);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setCursor(10, 103);
  tft.print("Senha:");

  tft.setCursor(10, 124);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print(AP_PASS);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  displayCentered("Depois acesse:", 162, 1, TFT_WHITE);
  displayCentered("192.168.4.1", 184, 2, TFT_CYAN);
  displayCentered("OTA: /update", 218, 1, TFT_ORANGE);
}

void drawConnectionFailedScreen()
{
  tft.fillScreen(TFT_BLACK);

  displayCentered("Falhou!", 80, 2, TFT_RED);
  displayCentered("Iniciando setup...", 120, 1, TFT_WHITE);

  delay(1500);
}

// ======================================================
// DASHBOARD DRAW
// ======================================================

String getTimeText()
{
  time_t now = time(nullptr);

  if (now < 100000)
  {
    return "--:--";
  }

  struct tm* timeInfo = localtime(&now);

  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);

  return String(buffer);
}

void drawDegreeTempSprite(TFT_eSprite& spr, int x, int y, int temp, uint16_t color)
{
  spr.setTextSize(2);
  spr.setTextColor(color, TFT_BLACK);

  String value = String(temp);

  spr.setCursor(x, y);
  spr.print(value);

  int numberWidth = spr.textWidth(value);

  spr.drawCircle(x + numberWidth + 5, y + 4, 2, color);

  spr.setCursor(x + numberWidth + 11, y);
  spr.print("C");
}

void drawCloudIconSprite(TFT_eSprite& spr, int x, int y, uint16_t color)
{
  spr.drawCircle(x + 8, y + 9, 7, color);
  spr.drawCircle(x + 17, y + 7, 9, color);
  spr.drawCircle(x + 27, y + 11, 7, color);
  spr.drawLine(x + 7, y + 16, x + 28, y + 16, color);
}

void drawHeader()
{
  TFT_eSprite header = TFT_eSprite(&tft);
  header.setColorDepth(8);

  void* result = header.createSprite(DISPLAY_WIDTH, HEADER_H);

  if (result == nullptr)
  {
    // Fallback tosco, mas evita morrer se faltar heap.
    tft.fillRect(0, 0, DISPLAY_WIDTH, HEADER_H, TFT_BLACK);
    displayCentered(getTimeText(), 18, 5, TFT_WHITE);
    tft.drawLine(12, 96, 228, 96, TFT_DARKGREY);
    return;
  }

  header.fillSprite(TFT_BLACK);

  String timeText = getTimeText();

  header.setTextSize(5);
  header.setTextColor(TFT_WHITE, TFT_BLACK);

  int timeWidth = header.textWidth(timeText);
  header.setCursor((DISPLAY_WIDTH - timeWidth) / 2, 10);
  header.print(timeText);

  if (hasWeather)
  {
    int tempToShow = (int)round(weatherTemp);

    drawDegreeTempSprite(header, 36, 66, tempToShow, TFT_ORANGE);
    drawCloudIconSprite(header, 91, 67, TFT_WHITE);

    header.setTextSize(2);
    header.setTextColor(TFT_WHITE, TFT_BLACK);
    header.setCursor(132, 66);
    header.print(weatherText);
  }
  else
  {
    header.setTextSize(2);
    header.setTextColor(TFT_DARKGREY, TFT_BLACK);

    String text = "--C  Clima";
    int w = header.textWidth(text);
    header.setCursor((DISPLAY_WIDTH - w) / 2, 66);
    header.print(text);
  }

  header.drawLine(12, 96, 228, 96, TFT_DARKGREY);
  header.pushSprite(0, 0);
  header.deleteSprite();

  lastTimeDrawn = timeText;
  lastWeatherDrawn = hasWeather ? String(weatherTemp, 1) + weatherText : "none";
}

uint16_t colorByValue(int value, uint16_t normalColor)
{
  if (value >= 90)
  {
    return TFT_RED;
  }

  if (value >= 75)
  {
    return TFT_ORANGE;
  }

  return normalColor;
}

void drawProgressBarOnSprite(TFT_eSprite& spr, int x, int y, int w, int h, int value, uint16_t color)
{
  if (value < 0) value = 0;
  if (value > 100) value = 100;

  int fillWidth = (w * value) / 100;

  spr.fillRoundRect(x, y, w, h, 4, TFT_DARKGREY);

  if (fillWidth > 0)
  {
    spr.fillRoundRect(x, y, fillWidth, h, 4, color);
  }
}

void drawMetricRow(int y, const String& label, int value, uint16_t baseColor)
{
  uint16_t barColor = colorByValue(value, baseColor);

  if (metricSpriteReady)
  {
    metricSprite.fillSprite(TFT_BLACK);

    metricSprite.setTextSize(2);
    metricSprite.setTextColor(TFT_WHITE, TFT_BLACK);

    metricSprite.setCursor(0, 4);
    metricSprite.print(label);

    String percentText = String(value) + "%";
    int percentWidth = metricSprite.textWidth(percentText);

    metricSprite.setCursor(METRIC_W - percentWidth, 4);
    metricSprite.print(percentText);

    drawProgressBarOnSprite(metricSprite, 64, 13, 112, 10, value, barColor);

    metricSprite.pushSprite(METRIC_X, y);
    return;
  }

  // Fallback sem sprite.
  tft.fillRect(METRIC_X, y, METRIC_W, METRIC_H, TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(METRIC_X, y + 4);
  tft.print(label);

  String percentText = String(value) + "%";
  int percentWidth = tft.textWidth(percentText);

  tft.setCursor(DISPLAY_WIDTH - percentWidth - 12, y + 4);
  tft.print(percentText);

  int fillWidth = (112 * value) / 100;
  tft.fillRoundRect(METRIC_X + 64, y + 13, 112, 10, 4, TFT_DARKGREY);
  tft.fillRoundRect(METRIC_X + 64, y + 13, fillWidth, 10, 4, barColor);
}

bool hasRecentPcMetrics()
{
  if (lastPcMetricsReceived == 0)
  {
    return false;
  }

  return millis() - lastPcMetricsReceived <= PC_METRICS_TIMEOUT_MS;
}

int clampPercentInt(int value)
{
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

void drawFooter()
{
  tft.fillRect(0, 218, DISPLAY_WIDTH, 22, TFT_BLACK);

  tft.drawLine(12, 220, 228, 220, TFT_DARKGREY);

  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

  String pcText = hasRecentPcMetrics() ? "PC ONLINE" : "PC WAIT";

  tft.setCursor(12, 228);
  tft.print(pcText);

  String wifiText = isApMode ? "SETUP" : "WiFi OK";
  int wifiWidth = tft.textWidth(wifiText);

  tft.setCursor(DISPLAY_WIDTH - wifiWidth - 12, 228);
  tft.print(wifiText);
}

void drawDashboardBase()
{
  tft.fillScreen(TFT_BLACK);

  lastTimeDrawn = "";
  lastWeatherDrawn = "";

  lastCpuDrawn = -1;
  lastRamDrawn = -1;
  lastGpuDrawn = -1;

  drawHeader();

  drawMetricRow(CPU_Y, "CPU", cpuCurrent, TFT_BLUE);
  drawMetricRow(RAM_Y, "RAM", ramCurrent, TFT_GREEN);
  drawMetricRow(GPU_Y, "GPU", gpuCurrent, TFT_CYAN);

  lastCpuDrawn = cpuCurrent;
  lastRamDrawn = ramCurrent;
  lastGpuDrawn = gpuCurrent;

  drawFooter();
}

void updateHeaderIfNeeded()
{
  String timeText = getTimeText();
  String weatherState = hasWeather ? String(weatherTemp, 1) + weatherText : "none";

  if (timeText != lastTimeDrawn || weatherState != lastWeatherDrawn)
  {
    drawHeader();
  }
}

void updateMetricsIfNeeded()
{
  if (cpuCurrent != lastCpuDrawn)
  {
    drawMetricRow(CPU_Y, "CPU", cpuCurrent, TFT_BLUE);
    lastCpuDrawn = cpuCurrent;
  }

  if (ramCurrent != lastRamDrawn)
  {
    drawMetricRow(RAM_Y, "RAM", ramCurrent, TFT_GREEN);
    lastRamDrawn = ramCurrent;
  }

  if (gpuCurrent != lastGpuDrawn)
  {
    drawMetricRow(GPU_Y, "GPU", gpuCurrent, TFT_CYAN);
    lastGpuDrawn = gpuCurrent;
  }
}

// ======================================================
// FAKE DATA COM ALVOS INDEPENDENTES
// ======================================================

int randomTargetNear(int current, int minValue, int maxValue, int maxDelta)
{
  int target = current + random(-maxDelta, maxDelta + 1);

  if (target < minValue)
  {
    target = minValue;
  }

  if (target > maxValue)
  {
    target = maxValue;
  }

  return target;
}

bool moveTowards(int& current, int target, int step)
{
  if (current == target)
  {
    return false;
  }

  if (current < target)
  {
    current += step;

    if (current > target)
    {
      current = target;
    }
  }
  else
  {
    current -= step;

    if (current < target)
    {
      current = target;
    }
  }

  return true;
}

void updateFakeTargets()
{
  unsigned long now = millis();

  if (now - lastCpuTargetUpdate >= CPU_TARGET_INTERVAL_MS)
  {
    lastCpuTargetUpdate = now;
    cpuTarget = randomTargetNear(cpuCurrent, 5, 96, 28);
  }

  if (now - lastRamTargetUpdate >= RAM_TARGET_INTERVAL_MS)
  {
    lastRamTargetUpdate = now;
    ramTarget = randomTargetNear(ramCurrent, 35, 90, 10);
  }

  if (now - lastGpuTargetUpdate >= GPU_TARGET_INTERVAL_MS)
  {
    lastGpuTargetUpdate = now;
    gpuTarget = randomTargetNear(gpuCurrent, 0, 98, 35);
  }
}

void animateFakeValues()
{
  unsigned long now = millis();

  if (now - lastAnimationUpdate < ANIMATION_INTERVAL_MS)
  {
    return;
  }

  lastAnimationUpdate = now;

  if (hasRecentPcMetrics())
  {
    moveTowards(cpuCurrent, cpuTarget, 10);
    moveTowards(ramCurrent, ramTarget, 6);
    moveTowards(gpuCurrent, gpuTarget, 12);
  }
  else
  {
    moveTowards(cpuCurrent, cpuTarget, 2);
    moveTowards(ramCurrent, ramTarget, 1);
    moveTowards(gpuCurrent, gpuTarget, 3);
  }
}

// ======================================================
// CLIMA
// ======================================================

String weatherCodeToText(int code)
{
  if (code == 0)
  {
    return "Limpo";
  }

  if (code == 1 || code == 2 || code == 3)
  {
    return "Nublado";
  }

  if (code == 45 || code == 48)
  {
    return "Neblina";
  }

  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82))
  {
    return "Chuva";
  }

  if (code >= 71 && code <= 77)
  {
    return "Neve";
  }

  if (code >= 95)
  {
    return "Tempest";
  }

  return "Clima";
}

String buildWeatherUrl()
{
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(WEATHER_LAT, 4);
  url += "&longitude=";
  url += String(WEATHER_LON, 4);
  url += "&current=temperature_2m,weather_code";
  url += "&timezone=";
  url += WEATHER_TIMEZONE;

  return url;
}

bool updateWeather()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setTimeout(2500);

  String url = buildWeatherUrl();

  if (!https.begin(client, url))
  {
    return false;
  }

  int httpCode = https.GET();

  if (httpCode != HTTP_CODE_OK)
  {
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error)
  {
    return false;
  }

  weatherTemp = doc["current"]["temperature_2m"] | 0.0f;
  int weatherCode = doc["current"]["weather_code"] | -1;

  weatherText = weatherCodeToText(weatherCode);
  hasWeather = true;

  return true;
}

// ======================================================
// SCAN DE REDES
// ======================================================

String htmlEscape(String value)
{
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}

void scanNetworks()
{
  scannedNetworks = "";

  int n = WiFi.scanNetworks();

  if (n <= 0)
  {
    scannedNetworks = "<p class='msg'>Nenhuma rede encontrada.</p>";
    return;
  }

  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    String safeSsid = htmlEscape(ssid);

    int rssi = WiFi.RSSI(i);
    bool open = WiFi.encryptionType(i) == ENC_TYPE_NONE;

    scannedNetworks += "<div class='net' onclick=\"sel('";
    scannedNetworks += safeSsid;
    scannedNetworks += "')\">";

    scannedNetworks += safeSsid;
    scannedNetworks += " (";
    scannedNetworks += String(rssi);
    scannedNetworks += " dBm)";

    if (open)
    {
      scannedNetworks += " [aberta]";
    }

    scannedNetworks += "</div>";
  }
}

// ======================================================
// HTML
// ======================================================

const char WIFI_SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Wi-Fi Setup</title>
<style>
body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:20px}
.card{background:#1f1f1f;padding:20px;border-radius:14px;max-width:460px;margin:auto}
h2{color:#ff9800;text-align:center;letter-spacing:1px}
.net{padding:12px;margin:7px 0;background:#2b2b2b;border-radius:8px;cursor:pointer}
.net:hover{background:#3a3a3a}
input,button,a{width:100%;padding:13px;margin:8px 0;border:none;border-radius:8px;box-sizing:border-box;font-size:16px}
input{background:#2b2b2b;color:#eee}
button{background:#ff9800;color:#111;font-weight:bold;cursor:pointer}
a{display:block;text-align:center;text-decoration:none;background:#327df6;color:#fff;font-weight:bold}
.danger{background:#a83232}
.msg{text-align:center;color:#aaa}
.small{text-align:center;color:#aaa;font-size:13px}
</style>
</head>
<body>
<div class='card'>
<h2>Wi-Fi Setup</h2>
<p class='small'>Selecione uma rede ou digite manualmente.</p>
<div id='nets'>%NETWORKS%</div>
<form action='/connect' method='POST'>
<input id='s' name='ssid' placeholder='Nome da rede Wi-Fi' required>
<input name='pass' type='password' placeholder='Senha da rede'>
<button type='submit'>Salvar e conectar</button>
</form>
<button onclick="location.href='/scan'">Buscar redes novamente</button>
<a href='/update'>Firmware Update OTA</a>
</div>
<script>
function sel(s){document.getElementById('s').value = s;}
</script>
</body>
</html>
)rawliteral";

const char HOME_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>PC Monitor</title>
<style>
body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:20px}
.card{background:#1f1f1f;padding:22px;border-radius:14px;max-width:460px;margin:auto}
h1{color:#ff9800;text-align:center}
.info{background:#2b2b2b;padding:12px;border-radius:8px;margin:10px 0}
a{display:block;width:100%;padding:14px;margin-top:12px;border-radius:8px;box-sizing:border-box;text-align:center;text-decoration:none;font-weight:bold}
.ota{background:#327df6;color:#fff}
.danger{background:#a83232;color:#fff}
.small{text-align:center;color:#aaa;font-size:13px}
</style>
</head>
<body>
<div class='card'>
<h1>PC Monitor</h1>
<p class='small'>ESP12F + ST7789 com Wi-Fi, OTA, hora real e clima real.</p>

<div class='info'><b>IP:</b> %IP%</div>
<div class='info'><b>SSID:</b> %SSID%</div>
<div class='info'><b>RSSI:</b> %RSSI% dBm</div>
<div class='info'><b>CPU fake:</b> %CPU%%</div>
<div class='info'><b>RAM fake:</b> %RAM%%</div>
<div class='info'><b>GPU fake:</b> %GPU%%</div>
<div class='info'><b>Temperatura:</b> %TEMP%</div>
<div class='info'><b>Heap livre:</b> %HEAP% bytes</div>

<a class='ota' href='/update'>Firmware Update OTA</a>
<a class='danger' href='/reset-wifi'>Apagar Wi-Fi salvo</a>
</div>
</body>
</html>
)rawliteral";

// ======================================================
// HANDLERS WEB
// ======================================================

void handleRoot()
{
  if (isApMode)
  {
    String page = FPSTR(WIFI_SETUP_HTML);
    page.replace("%NETWORKS%", scannedNetworks);
    server.send(200, "text/html", page);
    return;
  }

  String page = FPSTR(HOME_HTML);

  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%SSID%", WiFi.SSID());
  page.replace("%RSSI%", String(WiFi.RSSI()));
  page.replace("%CPU%", String(cpuCurrent));
  page.replace("%RAM%", String(ramCurrent));
  page.replace("%GPU%", String(gpuCurrent));
  page.replace("%TEMP%", hasWeather ? String(weatherTemp, 1) + " C" : "--");
  page.replace("%HEAP%", String(ESP.getFreeHeap()));

  server.send(200, "text/html", page);
}

void handleScan()
{
  scanNetworks();
  handleRoot();
}

void handleConnect()
{
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  ssid.trim();
  pass.trim();

  if (ssid.length() == 0)
  {
    server.send(400, "text/plain", "SSID vazio.");
    return;
  }

  saveCredentials(ssid, pass);

  server.send(200, "text/html",
    "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:Arial;background:#111;color:#eee;text-align:center;padding:40px}h2{color:#ff9800}</style>"
    "</head><body><h2>Wi-Fi salvo.</h2><p>O display vai reiniciar e tentar conectar.</p></body></html>"
  );

  delay(1200);
  ESP.restart();
}

void handleResetWifi()
{
  clearCredentials();

  server.send(200, "text/html",
    "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:Arial;background:#111;color:#eee;text-align:center;padding:40px}h2{color:#ff9800}</style>"
    "</head><body><h2>Wi-Fi apagado.</h2><p>O display vai reiniciar em modo setup.</p></body></html>"
  );

  delay(1200);
  ESP.restart();
}

void handleMetrics()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "application/json", "{\"ok\":false,\"error\":\"method_not_allowed\"}");
    return;
  }

  String body = server.arg("plain");

  if (body.length() == 0)
  {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty_body\"}");
    return;
  }

  StaticJsonDocument<192> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error)
  {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
    return;
  }

  if (!doc.containsKey("cpu") || !doc.containsKey("ram") || !doc.containsKey("gpu"))
  {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_fields\"}");
    return;
  }

  int cpu = clampPercentInt(doc["cpu"] | 0);
  int ram = clampPercentInt(doc["ram"] | 0);
  int gpu = clampPercentInt(doc["gpu"] | 0);

  cpuTarget = cpu;
  ramTarget = ram;
  gpuTarget = gpu;
  
  cpuCurrent = cpu;
  ramCurrent = ram;
  gpuCurrent = gpu;
  
  lastCpuDrawn = -1;
  lastRamDrawn = -1;
  lastGpuDrawn = -1;
  
  lastPcMetricsReceived = millis();

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStatus()
{
  String ip = isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String ssid = isApMode ? String(AP_SSID) : WiFi.SSID();

  String json = "{";
  json += "\"name\":\"PC Monitor\",";
  json += "\"mode\":\"";
  json += isApMode ? "AP" : "STA";
  json += "\",";
  json += "\"ip\":\"";
  json += ip;
  json += "\",";
  json += "\"ssid\":\"";
  json += ssid;
  json += "\",";
  json += "\"cpu\":";
  json += String(cpuCurrent);
  json += ",";
  json += "\"ram\":";
  json += String(ramCurrent);
  json += ",";
  json += "\"gpu\":";
  json += String(gpuCurrent);
  json += ",";
  json += "\"pcOnline\":";
  json += hasRecentPcMetrics() ? "true" : "false";
  json += ",";
  json += "\"lastPcMetricsAgeMs\":";
  json += lastPcMetricsReceived == 0 ? "null" : String(millis() - lastPcMetricsReceived);
  json += ",";
  json += "\"temperature\":";
  json += hasWeather ? String(weatherTemp, 1) : "null";
  json += ",";
  json += "\"weather\":\"";
  json += weatherText;
  json += "\",";
  json += "\"heap\":";
  json += String(ESP.getFreeHeap());
  json += ",";
  json += "\"flashSize\":";
  json += String(ESP.getFlashChipRealSize());
  json += "}";

  server.send(200, "application/json", json);
}

// ======================================================
// SERVIDOR WEB + OTA
// ======================================================

void setupServer()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/reset-wifi", HTTP_GET, handleResetWifi);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/metrics", HTTP_POST, handleMetrics);

  httpUpdater.setup(&server, "/update", OTA_USER, OTA_PASS);

  server.begin();
}

// ======================================================
// WIFI
// ======================================================

bool tryConnectWiFi(const String& ssid, const String& pass)
{
  drawConnectingNetwork(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  {
    delay(500);
    attempts++;
    yield();
  }

  return WiFi.status() == WL_CONNECTED;
}

bool connectToWiFi()
{
  String ssid;
  String pass;

  bool hasSavedCredentials = loadCredentials(ssid, pass);

  if (!hasSavedCredentials)
  {
    ssid = DEFAULT_WIFI_SSID;
    pass = DEFAULT_WIFI_PASS;
  }

  if (ssid.length() == 0)
  {
    return false;
  }

  bool connected = tryConnectWiFi(ssid, pass);

  if (connected)
  {
    isApMode = false;
    return true;
  }

  drawConnectionFailedScreen();
  return false;
}

void startApMode()
{
  isApMode = true;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  delay(300);

  scanNetworks();
  drawApScreen();
}

// ======================================================
// SETUP / LOOP
// ======================================================

void setup()
{
  Serial.begin(115200);
  delay(300);

  randomSeed(ESP.getCycleCount());

  initDisplay();
  initSprites();

  drawBootScreen();

  bool connected = connectToWiFi();

  if (!connected)
  {
    startApMode();
  }

  setupServer();

  if (!isApMode)
  {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

    drawDashboardBase();

    // Atualiza clima uma vez depois de exibir dashboard.
    updateWeather();
    lastWeatherUpdate = millis();

    updateHeaderIfNeeded();
  }

  Serial.println();
  Serial.println("=====================================");
  Serial.println("PC Monitor + Sprite + OTA iniciado");
  Serial.println("=====================================");

  if (isApMode)
  {
    Serial.println("Modo: AP Setup");
    Serial.print("Wi-Fi: ");
    Serial.println(AP_SSID);
    Serial.print("Senha: ");
    Serial.println(AP_PASS);
    Serial.println("Pagina: http://192.168.4.1/");
    Serial.println("OTA:    http://192.168.4.1/update");
  }
  else
  {
    Serial.println("Modo: Wi-Fi conectado");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Pagina: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
    Serial.print("OTA:    http://");
    Serial.print(WiFi.localIP());
    Serial.println("/update");
  }

  Serial.print("Flash real: ");
  Serial.println(ESP.getFlashChipRealSize());

  Serial.print("Heap livre: ");
  Serial.println(ESP.getFreeHeap());

  Serial.print("Metric sprite: ");
  Serial.println(metricSpriteReady ? "OK" : "FALHOU");
}

void loop()
{
  server.handleClient();

  if (isApMode)
  {
    return;
  }

  unsigned long now = millis();

  if (!hasRecentPcMetrics() && USE_FAKE_METRICS_WHEN_PC_OFFLINE)
  {
    updateFakeTargets();
  }
  
  animateFakeValues();
  updateMetricsIfNeeded();

  if (now - lastClockCheck >= CLOCK_CHECK_INTERVAL_MS)
  {
    lastClockCheck = now;
    updateHeaderIfNeeded();
  }

  if (now - lastFooterUpdate >= FOOTER_UPDATE_INTERVAL_MS)
  {
    lastFooterUpdate = now;
    drawFooter();
  }

  if (now - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL_MS)
  {
    lastWeatherUpdate = now;

    if (updateWeather())
    {
      updateHeaderIfNeeded();
    }
  }
}