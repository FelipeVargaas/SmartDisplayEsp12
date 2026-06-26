# AGENTS.md — SMARTDISPLAYESP12

Este arquivo deve ficar na raiz do workspace:

```text
SMARTDISPLAYESP12/
├── AGENTS.md
├── README.md
├── Esp12_SmartDisplay/
└── SmartDisplayPCAgent/
```

Este workspace contém dois projetos que fazem parte do mesmo produto:

- `Esp12_SmartDisplay`: firmware PlatformIO para ESP8266 / ESP12F.
- `SmartDisplayPCAgent`: aplicativo desktop Avalonia / .NET 9.
- A comunicação entre os dois lados deve ser tratada como parte crítica do sistema.

O objetivo principal é manter firmware e aplicativo desktop coerentes entre si. Qualquer alteração em protocolo, comandos, payloads, estados, endpoints, formato de dados ou fluxo de comunicação deve considerar os dois projetos.

---

## Diretrizes atuais do TinyDash

Estas diretrizes refletem decisões recentes do produto e devem orientar novas alterações.

### Comunicação PC Agent <-> ESP

- O PC Agent deve ser gentil com o ESP8266: evitar polling agressivo e evitar requisições concorrentes desnecessárias.
- Requisições HTTP para o ESP devem ser coordenadas/serializadas quando houver risco de colisão entre `/metrics`, `/status`, `/theme`, OTA, clima ou outras rotas.
- `/metrics` pode continuar frequente quando o envio de telemetria estiver ativo, pois é o canal de dados vivos do display.
- `/status` deve ser usado para diagnóstico, refresh de estado e confirmação de tema/dispositivo. Não usar `/status` como polling agressivo.
- Ao iniciar o PC Agent, é desejável obter um `/status` inicial cedo para preencher tema, IP e estado real do ESP sem deixar a UI com dados genéricos por muito tempo.
- Quando um campo opcional precisa ser limpo no firmware, enviar limpeza explícita (`null` ou string vazia, conforme o contrato existente). Não omitir o campo se a omissão fizer o ESP preservar valor antigo.

### Estado visual e dados vencidos

- O firmware deve ser honesto visualmente: dado inexistente, vencido ou sem comunicação recente deve aparecer como `--`, não como mock ou último valor aparentemente real.
- Não manter FPS, frametime, temperaturas ou métricas antigas na tela como se ainda fossem atuais depois que o PC Agent parar de enviar dados recentes.
- Mocks visuais só devem existir quando explicitamente pedidos para demonstração/animação. Em temas operacionais, preferir `--`.
- O estado `pcOnline`/métricas recentes deve governar se CPU, RAM, GPU, disco, FPS, frametime e temperaturas são mostrados como reais ou como `--`.

### Clima

- Clima continua sendo responsabilidade do firmware. Não mover a busca de clima para o PC Agent como atalho.
- Requisição de clima deve acontecer apenas em temas que usam clima.
- Ao entrar em tema que usa clima, tentar obter clima com cautela no início; depois de obter sucesso, usar intervalo longo.
- Evitar que clima concorra de forma agressiva com `/metrics`, `/status`, OTA ou outras rotas sensíveis.

### Tema Gamer

- FPS e frametime só fazem sentido quando há jogo/RTSS válido.
- Quando o jogo fecha ou o RTSS deixa de fornecer dados válidos, o PC Agent deve limpar `game`, `fps`, `frametime` e `source` no payload enviado ao ESP.
- Sem jogo válido, o display deve mostrar `GAME HUD`, `RTSS OFF` e `--` para FPS/frametime.
- Sem métricas recentes do PC, o Gamer HUD deve mostrar `--` para CPU, RAM, GPU, GPU temp, FPS e frametime.
- O nome amigável do jogo é responsabilidade do PC Agent. O firmware recebe apenas o nome já normalizado no campo `game`.
- Mapeamento de processo para nome amigável deve ser local ao PC Agent, em JSON local, sem mudar o protocolo do ESP.

### UI do PC Agent

- Manter visual escuro, limpo, compacto e operacional.
- Controles clicáveis devem ter cursor de mão e feedback de hover claro, especialmente tabs, botões e checkboxes.
- Botões destrutivos ou sensíveis, como reset de Wi-Fi salvo e firmware update OTA, devem ter confirmação antes da ação real.
- A aba Device deve priorizar diagnóstico real do ESP: conexão, tema ativo, PC online, heap, reset, clima e status atualizado.
- A aba Dashboard deve mostrar dados locais e estado de envio sem parecer uma landing page.
- Configurações locais do PC Agent, como mapeamento de nomes de jogos, devem ficar no PC e não exigir mudança de firmware.

### Build e execução local

- No VS Code, o debug do PC Agent pode usar uma saída isolada como `.vscode/bin/pcagent` para evitar lock do `bin/Debug/net9.0` quando o Visual Studio ou uma execução anterior estiver usando a DLL.
- Se o build falhar por arquivo em uso, investigar processo segurando `SmartDisplayPcAgent.dll` antes de alterar código de aplicação.
- Não fazer upload/flash do firmware sem autorização explícita, mesmo quando o build do PlatformIO passar.

---

## Visão geral do sistema

### Firmware

Projeto:

```text
Esp12_SmartDisplay/
```

Características:

- PlatformIO.
- ESP8266 / ESP12F.
- Arduino framework.
- Display ST7789V TFT 240x240 SPI.
- Biblioteca gráfica TFT_eSPI.
- OTA via ESP8266HTTPUpdateServer.
- Rota de atualização `/update`.
- Wi-Fi com fallback AP para configuração.

### Aplicativo desktop

Projeto:

```text
SmartDisplayPcAgent/
```

Características:

- Avalonia.
- .NET 9.
- Projeto C#.
- Arquivos `.axaml`.
- Estrutura com `Clients`, `Models`, `Services`, `ViewModels` e `Views`.

---

## Regra de ouro

Antes de alterar qualquer coisa, classifique a tarefa:

```text
1. Só firmware.
2. Só desktop.
3. Firmware + desktop.
4. Documentação/configuração.
```

Se a alteração envolver comunicação entre o app e o ESP12F, trate como:

```text
Firmware + desktop
```

Não altere só um lado fingindo que o outro vai adivinhar. Firmware não tem telepatia. Ainda.

---

## Regras globais obrigatórias

- Não fazer alterações fora do escopo pedido.
- Não fazer refactor cosmético junto com bugfix.
- Não renomear arquivos, pastas, classes, comandos, endpoints ou propriedades sem necessidade clara.
- Não alterar protocolo de comunicação sem atualizar todos os lados afetados.
- Não inventar arquitetura grande para resolver problema pequeno.
- Preferir mudanças pequenas, diretas, revisáveis e fáceis de reverter.
- Preservar o estilo atual dos projetos.
- Não adicionar dependências novas sem justificativa técnica clara.
- Não alterar credenciais, nomes de rede, senhas, tokens, secrets ou configurações sensíveis.
- Não registrar credenciais ou dados sensíveis em logs.
- Não remover logs úteis de diagnóstico sem motivo.
- Não fazer upload, flash, deploy, publish ou release sem autorização explícita.
- Sempre informar quais arquivos foram alterados.
- Sempre informar quais comandos de validação foram executados.
- Se um comando falhar, mostrar o erro real. Não mascarar falha.

---

## Regras para comunicação entre desktop e firmware

Quando uma tarefa envolver qualquer um destes itens:

- Serial.
- HTTP.
- WebSocket.
- TCP.
- UDP.
- MQTT.
- BLE.
- Endpoints.
- Comandos.
- Telemetria.
- Status.
- Configuração.
- Payload JSON.
- Payload binário.
- Códigos de erro.
- Estados do dispositivo.

Então:

- Verificar o código dos dois lados.
- Manter nomes de comandos consistentes.
- Manter nomes de propriedades consistentes.
- Manter tipos de dados consistentes.
- Manter unidades explícitas: `ms`, `s`, `°C`, `%`, `V`, `A`, `bytes`, `rpm`, etc.
- Manter códigos de erro e estados documentáveis.
- Tratar timeout, desconexão e resposta inválida.
- Tratar dispositivo offline.
- Tratar dispositivo reiniciando.
- Tratar dispositivo em modo AP fallback.
- Tratar firmware antigo, quando aplicável.
- Não quebrar compatibilidade sem avisar.

Antes de implementar mudança no protocolo, responder:

```text
Quem envia?
Quem recebe?
Qual é o comando?
Qual é o payload?
Qual é a resposta esperada?
Qual é o timeout?
O que acontece em erro?
O que acontece se o firmware for uma versão antiga?
O que acontece se o app for uma versão antiga?
```

Se existir documentação de protocolo, atualizar. Se não existir, sugerir criar um arquivo como:

```text
docs/protocolo.md
```

---

## Regras para o projeto PlatformIO / ESP8266

Escopo:

```text
Esp12_SmartDisplay/
```

Regras obrigatórias herdadas do projeto atual:

- Não remover OTA.
- Não remover rota `/update`.
- Não remover fallback AP de configuração Wi-Fi.
- Não alterar pinagem do display/backlight sem pedir confirmação.
- Não alterar credenciais, nomes de rede ou senhas.
- Não alterar comportamento visual sem pedido explícito.
- Não alterar layout do dashboard sem pedido explícito.
- Não migrar para LittleFS/SPIFFS sem pedido explícito.
- Não criar classes complexas sem necessidade.
- Preferir módulos simples `.h` / `.cpp`.
- Manter baixo uso de RAM.
- Não usar sprite fullscreen no ESP8266.
- Assets pequenos devem ficar em `PROGMEM`.
- Corrigir apenas erros necessários para o build passar, salvo instrução contrária.
- Não fazer upload/flash sem autorização explícita.

Boas práticas específicas:

- Evitar alocação dinâmica desnecessária.
- Evitar uso excessivo de `String` em caminhos críticos de memória.
- Preferir buffers fixos quando fizer sentido e for seguro.
- Validar tamanho de buffers.
- Evitar delays longos no `loop()`.
- Não bloquear Wi-Fi, OTA ou servidor web.
- Manter o servidor OTA acessível.
- Manter rota `/update` funcional.
- Manter fallback AP funcional.
- Não mudar pinos sem confirmação humana.
- Não aumentar uso de RAM sem necessidade.
- Não introduzir sprites grandes.
- Não usar sprite fullscreen no ESP8266.
- Guardar assets pequenos em `PROGMEM`.

Comando de validação obrigatório para firmware:

```bash
pio run -d Esp12_SmartDisplay
```

Se estiver executando dentro da pasta `Esp12_SmartDisplay`, pode usar:

```bash
pio run
```

Não executar estes comandos sem autorização explícita:

```bash
pio run --target upload
pio run -t upload
pio device monitor
```

`pio device monitor` só deve ser usado quando o usuário pedir ou autorizar, porque pode interferir no uso da porta serial.

---

## Regras para o projeto Avalonia / .NET 9

Escopo:

```text
SmartDisplayPCAgent/
```

Regras obrigatórias:

- Manter Avalonia.
- Manter .NET 9, salvo instrução explícita em contrário.
- Não migrar para WPF, WinUI, MAUI, Uno ou outro framework.
- Não alterar layout visual sem pedido explícito.
- Não alterar comportamento visual sem pedido explícito.
- Preservar arquivos `.axaml`.
- Preservar bindings existentes.
- Preservar comandos usados por Views e ViewModels.
- Evitar lógica de negócio dentro de Views.
- Preferir lógica em `Services`, `Clients` ou `ViewModels`, conforme padrão existente.
- Não bloquear a UI thread.
- Usar `async/await` para I/O, rede, serial, timers e operações demoradas.
- Evitar `Thread.Sleep` em código de UI.
- Evitar estado global mutável sem necessidade.
- Não adicionar bibliotecas grandes por conveniência pequena.
- Corrigir somente erros necessários para o build passar, salvo instrução contrária.

Boas práticas específicas:

- `Clients`: comunicação externa, HTTP, serial, APIs ou dispositivo.
- `Services`: regras de aplicação e orquestração.
- `Models`: dados, DTOs e estruturas simples.
- `ViewModels`: estado e comandos da UI.
- `Views`: layout Avalonia, sem regra de negócio pesada.

Ao mexer em `.axaml`:

- Verificar `DataContext`.
- Verificar `Binding`.
- Verificar nomes de controles.
- Verificar comandos.
- Verificar conversores.
- Verificar code-behind correspondente.

Ao mexer em ViewModels:

- Preservar notificações de propriedade.
- Preservar comandos existentes.
- Não quebrar bindings silenciosamente.
- Evitar lógica de rede direta no ViewModel quando já existir `Service` ou `Client`.

Comando de validação obrigatório para desktop:

```bash
dotnet build SmartDisplayPCAgent/SmartDisplayPCAgent.slnx
```

Se o `.slnx` não for suportado pelo SDK instalado, usar:

```bash
dotnet build SmartDisplayPCAgent/SmartDisplayPCAgent.csproj
```

---

## Validação por tipo de alteração

### Alteração só no firmware

Executar:

```bash
pio run -d Esp12_SmartDisplay
```

### Alteração só no desktop

Executar:

```bash
dotnet build SmartDisplayPCAgent/SmartDisplayPCAgent.slnx
```

Fallback:

```bash
dotnet build SmartDisplayPCAgent/SmartDisplayPCAgent.csproj
```

### Alteração no firmware e no desktop

Executar:

```bash
pio run -d Esp12_SmartDisplay
dotnet build SmartDisplayPCAgent/SmartDisplayPCAgent.slnx
```

Fallback para desktop:

```bash
dotnet build SmartDisplayPCAgent/SmartDisplayPCAgent.csproj
```

### Alteração só em documentação

Não é obrigatório build, mas informar que não foi necessário.

---

## Fluxo esperado para o Codex

Ao receber uma tarefa:

1. Ler esta orientação.
2. Identificar o escopo da mudança.
3. Listar rapidamente os arquivos prováveis.
4. Fazer a menor alteração viável.
5. Evitar refactor não solicitado.
6. Verificar impacto no outro projeto.
7. Executar validação aplicável.
8. Mostrar resumo objetivo.
9. Mostrar erros reais, se houver.
10. Não fazer flash, upload, publish ou deploy sem autorização.

Formato recomendado de resposta após alteração:

```text
Resumo:
- O que foi alterado.

Arquivos:
- caminho/arquivo1
- caminho/arquivo2

Validação:
- comando executado
- resultado

Observações:
- riscos, pendências ou próximos passos
```

---

## Política de escopo

Se o pedido for pequeno, a mudança deve ser pequena.

Exemplos:

```text
Pedido: corrigir build.
Permitido: corrigir includes, namespaces, referências quebradas.
Não permitido: reestruturar o projeto.
```

```text
Pedido: adicionar um campo no status.
Permitido: adicionar campo no firmware, modelo no desktop e exibição se solicitado.
Não permitido: trocar o protocolo inteiro.
```

```text
Pedido: ajustar layout.
Permitido: alterar somente layout relacionado.
Não permitido: mexer no OTA, Wi-Fi ou protocolo.
```

---

## Segurança

- Não expor secrets.
- Não salvar senhas novas hardcoded.
- Não alterar credenciais Wi-Fi.
- Não alterar autenticação de OTA sem pedido explícito.
- Não remover proteções existentes.
- Não abrir endpoints administrativos novos sem explicar finalidade e risco.
- Não reduzir validações para “fazer funcionar”.
- Não registrar payloads sensíveis em logs.
- Não alterar nomes de rede ou senhas.
- Não alterar tokens ou chaves.

---

## Performance e recursos

### ESP8266

- RAM é recurso crítico.
- Evitar buffers grandes.
- Evitar sprite fullscreen.
- Evitar fragmentação de heap.
- Evitar `String` em loops críticos.
- Evitar delays longos.
- Preservar Wi-Fi e OTA responsivos.
- Preferir assets pequenos em `PROGMEM`.

### Desktop

- Não travar UI.
- Não fazer polling agressivo sem necessidade.
- Não usar timers sem cancelamento.
- Cancelar operações longas quando aplicável.
- Tratar exceções de I/O.
- Tratar reconexão com o dispositivo.
- Exibir erro útil quando comunicação falhar, se a tarefa envolver UI.

---

## Convenções de código

### C# / Avalonia

- Código claro e direto.
- Nomes descritivos.
- Preferir `async/await`.
- Evitar `async void`, exceto handlers de UI quando necessário.
- Evitar lógica pesada em code-behind.
- Seguir a estrutura existente do projeto.
- Não introduzir abstração prematura.
- Não introduzir dependência externa sem necessidade.

### C/C++ / PlatformIO

- Código simples e econômico.
- Preferir funções pequenas.
- Preferir `.h` / `.cpp` quando separar fizer sentido.
- Validar buffers.
- Cuidado com heap.
- Cuidado com conversões.
- Evitar delays longos.
- Preservar OTA, Wi-Fi e servidor web.
- Preservar padrões existentes.

---

## Arquivos e pastas que merecem cuidado

### Firmware

```text
Esp12_SmartDisplay/platformio.ini
Esp12_SmartDisplay/src/
Esp12_SmartDisplay/include/
Esp12_SmartDisplay/lib/
```

Cuidado especial com:

- configuração de placa;
- bibliotecas;
- pinagem;
- TFT_eSPI;
- OTA;
- Wi-Fi;
- fallback AP;
- rota `/update`;
- renderização do display.

### Desktop

```text
SmartDisplayPCAgent/SmartDisplayPCAgent.slnx
SmartDisplayPCAgent/SmartDisplayPCAgent.csproj
SmartDisplayPCAgent/App.axaml
SmartDisplayPCAgent/App.axaml.cs
SmartDisplayPCAgent/Program.cs
SmartDisplayPCAgent/ViewLocator.cs
SmartDisplayPCAgent/Clients/
SmartDisplayPCAgent/Models/
SmartDisplayPCAgent/Services/
SmartDisplayPCAgent/ViewModels/
SmartDisplayPCAgent/Views/
```

Cuidado especial com:

- bindings;
- ViewModels;
- Services;
- Clients;
- modelos compartilhados com o protocolo;
- comunicação com dispositivo;
- build do projeto.

---

## Quando pedir confirmação

Pedir confirmação antes de:

- alterar pinagem;
- remover OTA;
- alterar `/update`;
- alterar fallback AP;
- alterar credenciais;
- fazer flash/upload;
- publicar/release;
- mudar framework;
- trocar protocolo inteiro;
- migrar armazenamento;
- adicionar dependência grande;
- alterar layout visual não solicitado;
- alterar comportamento visual não solicitado;
- mudar nomes públicos de comandos/endpoints.

---

## O que não fazer

- Não transformar correção pequena em reescrita.
- Não mexer no firmware para resolver problema só do desktop.
- Não mexer no desktop para resolver problema só do firmware.
- Não alterar protocolo sem revisar os dois lados.
- Não ignorar erro de build.
- Não esconder warnings introduzidos pela alteração.
- Não fazer upload/flash sem autorização.
- Não alterar credenciais.
- Não remover OTA.
- Não remover rota `/update`.
- Não remover fallback AP.
- Não alterar pinagem sem confirmação.
- Não trocar Avalonia por outro framework.
- Não bloquear UI thread.
- Não usar sprite fullscreen no ESP8266.
- Não criar arquitetura enterprise para projeto embarcado pequeno. Java já sofreu o bastante.

---

## Prioridade entre AGENTS.md

Este arquivo na raiz define as regras gerais do workspace.

Se existir outro `AGENTS.md` dentro de uma subpasta, ele complementa este arquivo para aquela subpasta específica.

Exemplo:

```text
SMARTDISPLAYESP12/AGENTS.md
Esp12_SmartDisplay/AGENTS.md
SmartDisplayPCAgent/AGENTS.md
```

Prioridade prática:

1. Regras da subpasta específica.
2. Regras deste arquivo raiz.
3. Instruções diretas do usuário.

Em caso de conflito, perguntar antes de alterar algo crítico.

