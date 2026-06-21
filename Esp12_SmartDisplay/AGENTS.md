# Smart Display ESP12F PC Monitor

Projeto PlatformIO para ESP8266 / ESP12F usando Arduino framework.

## Hardware

- MCU: ESP12F / ESP8266
- Display: ST7789V TFT 240x240 SPI
- Biblioteca gráfica: TFT_eSPI
- OTA via ESP8266HTTPUpdateServer
- Wi-Fi com fallback AP setup

## Regras obrigatórias

- Não remover OTA.
- Não remover rota `/update`.
- Não remover fallback AP de configuração Wi-Fi.
- Não alterar pinagem do display/backlight sem pedir confirmação.
- Não alterar credenciais, nomes de rede ou senhas nesta tarefa.
- Não alterar comportamento visual nesta tarefa.
- Não alterar layout do dashboard nesta tarefa.
- Não migrar para LittleFS/SPIFFS nesta tarefa.
- Não criar classes complexas sem necessidade.
- Preferir módulos simples `.h`/`.cpp`.
- Manter baixo uso de RAM.
- Não usar sprite fullscreen no ESP8266.
- Assets pequenos devem ficar em `PROGMEM`.
- Após alterações, rodar `pio run`.
- Corrigir apenas erros necessários para o build passar.
- Não fazer upload/flash sem autorização explícita.