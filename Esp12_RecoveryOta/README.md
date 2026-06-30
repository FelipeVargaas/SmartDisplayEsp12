# TinyDash Recovery OTA

Firmware minimo para recuperar atualizacao OTA quando o firmware principal estiver com pouca memoria.

Ele mantem apenas:

- Wi-Fi STA usando credenciais salvas na EEPROM ou os defaults do firmware principal.
- Fallback AP para configurar Wi-Fi.
- WebServer HTTP.
- `/update` com o mesmo usuario/senha OTA do firmware principal.
- `/status` com informacoes basicas de heap/IP.
- `/reset-wifi` para limpar credenciais salvas.

Build:

```bash
pio run -d Esp12_RecoveryOta
```

Binario gerado:

```text
Esp12_RecoveryOta/.pio/build/esp12e_recovery/firmware.bin
```

Fluxo sugerido:

1. Fechar o PC Agent.
2. Reiniciar o ESP.
3. Abrir `http://IP_DO_ESP/update`.
4. Enviar este `firmware.bin` recovery.
5. Apos reiniciar no recovery, abrir novamente `/update`.
6. Enviar o firmware principal corrigido.
