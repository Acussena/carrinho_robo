# Testes e demonstração

Legenda: ✅ passou · ⚠️ passou com ressalva · ❌ falhou · ⏳ a executar (pelo grupo, na gravação do vídeo)

Testes feitos em 14/11/2026 com o carrinho real: firmwares da GS gravados na R3 (COM9) e na ESP32-S3 (COM10), painel ligado ao robô pelo Wi-Fi (`http://fiscal.local`) e IA com o Gemini (chave gratuita do Google AI Studio).

Evidências em [`logs/`](logs):

| Arquivo | Conteúdo |
|---|---|
| [teste_borda_mesa_telemetria.txt](logs/teste_borda_mesa_telemetria.txt) | telemetria comentada do teste da borda (piso, distância, decisão, manobra) |
| [ocorrencias_teste_borda_mesa.csv](logs/ocorrencias_teste_borda_mesa.csv) | ocorrências com a análise do LLM (gravidade, alerta falado, recomendação) |
| [teste_borda_mesa_eventos_esp32.json](logs/teste_borda_mesa_eventos_esp32.json) | ocorrências cruas registradas na ESP32 (`/eventos`) |
| [relatorio_ia_teste_borda_mesa.md](logs/relatorio_ia_teste_borda_mesa.md) | relatório para a prefeitura gerado pelo LLM a partir do teste |

## 1. Integração hardware ↔ software

| # | Teste | Resultado |
|---|---|---|
| H1 | Compilação dos firmwares (arduino-cli) | ✅ R3: 25.242 bytes (78%), RAM 33% · ESP32-S3: 839 KB (25%), RAM 9% |
| H2 | Gravação das duas placas sem soltar o cabo Camera UART (ESP32 mantida em modo de gravação durante o upload da R3) | ✅ |
| H3 | R3 → ESP32 no formato da GS (`millis,cm,L,M,R,V10,desnivel,modo`) | ✅ `r3_ok: true`, piso e bateria chegando |
| H4 | ESP32 no Wi-Fi + API (`/status`, `/eventos`, `/cmd`) | ✅ IP 192.168.0.27, sinal −52 a −56 dBm, `fiscal.local` resolvendo |
| H5 | Painel recebendo a telemetria pelo Wi-Fi e comandando o robô (Iniciar/Parar) | ✅ |

## 2. Sensor adicional (piso)

| # | Teste | Esperado | Resultado |
|---|---|---|---|
| P1 | Leitura no chão | valores baixos e estáveis | ✅ **0 / 0 / 0** |
| P2 | Frente levantada ~10 cm (sem piso) | valores altos | ✅ **734–749** nos três sensores |
| P3 | Calibração | limiar entre P1 e P2 | ✅ limiar **550** (o padrão inicial, 850, ficava acima do valor sem piso); com a frente levantada: `desnivel: true` |
| P4 | Superfície escura | não confundir com desnível | ⚠️ uma mesa escura leu ~405 (abaixo de 550, ok). Faixa preta não testada: limitação documentada |

## 3. Comportamento autônomo

| # | Teste | Esperado | Resultado |
|---|---|---|---|
| A1 | **Borda da mesa = degrau/meio-fio** | para, LED roxo, ré + giro, alerta falado | ✅ **4 desníveis** detectados e evitados (parada → ré → giro), inclusive com só 1 sensor na borda durante o giro. O LLM classificou todos como gravidade **alta** ([telemetria](logs/teste_borda_mesa_telemetria.txt)) |
| A2 | Obstáculo à frente | desacelera e desvia | ✅ **7 obstáculos** (mão/objetos a 1–31 cm) → ré + giro. ⏳ repetir com uma caixa no vídeo |
| A3 | Trecho crítico | modo cauteloso | ✅ `trecho_critico` após 2 barreiras em 30 s; em seguida, "livre" a **vel 110** em vez de 150 |
| A4 | Robô levantado com patrulha ativa | não avança | ⏳ (P2 confirma a detecção; falta filmar as rodas paradas) |
| A5 | Caminho livre | frente rápido, LED verde | ✅ `livre -> dir 1 vel 150` |
| A6 | Standby | modelo roda, mas sem ocorrências | ✅ após o ajuste do item 6 |

## 4. IA (LLM) e painel

| # | Teste | Resultado |
|---|---|---|
| I1 | Análise de cada ocorrência real pelo LLM | ✅ 6 chamadas ao Gemini no teste; alertas como "Cuidado! Desnível perigoso à frente. Risco de queda para pedestres e cadeirantes." |
| I2 | Troca automática de modelo | ✅ `gemini-2.5-flash` descontinuado (404) → `gemini-3.5-flash`; durante o teste, troca para `gemini-3.5-flash-lite` quando o principal não respondeu |
| I3 | Repetição em menos de 8 s reaproveita a análise (poupa cota) | ✅ marcadas como "(repetição)" no CSV |
| I4 | Pergunta em linguagem natural | ✅ "Esta calçada é acessível para cadeirantes?" respondida com base nos dados |
| I5 | Relatório para a prefeitura | ✅ [relatorio_ia_teste_borda_mesa.md](logs/relatorio_ia_teste_borda_mesa.md) |
| I6 | Simulação sem robô e regras offline sem chave | ✅ (`painel.py --simular`) |
| I7 | LLM sem internet durante a patrulha | ⏳ desligar a internet do PC: as ocorrências devem seguir com `regras (offline)` |

## 5. Situações de falha

| # | Teste | Esperado | Resultado |
|---|---|---|---|
| F1 | Cabo R3 ↔ ESP32 solto | motores param em ≤ 1 s; `falha_r3`; ao reconectar, `r3_reconectada` | ⏳ |
| F2 | Wi-Fi caído | robô segue autônomo; painel avisa "sem dados do robô"; reconecta sozinho | ⏳ |
| F3 | R3 com firmware antigo | `sensor_piso_indisponivel` | não executado (exigiria regravar o CP2) |
| F4 | Chave da bateria desligada (só USB, 4,5 V) | sem alarme falso | ✅ painel mostra "desligada (só USB)", após o ajuste do item 6 |
| F5 | Sem chave do LLM | regras offline | ✅ |

## 6. Problemas encontrados nos testes e correções

| Problema observado | Causa | Correção |
|---|---|---|
| Desnível não era detectado com a frente levantada | sem piso o sensor lê ~740, abaixo do limiar inicial de 850 | calibração; limiar padrão **550** |
| Alerta de "bateria baixa" com o carrinho parado | chave da bateria desligada: a placa media só a USB (4,5 V) | abaixo de 5,5 V = "bateria desligada (só USB)", sem alarme |
| Obstáculos viravam ocorrência com o carrinho em standby | a ESP32 não sabia o modo da R3 | a R3 envia o modo; só há ocorrência durante a patrulha |
| Erro 404 do LLM | `gemini-2.5-flash` descontinuado para chaves novas | modelo `gemini-3.5-flash` + lista de reserva (404/429/503/timeout) |
| Carrinho encostava na cama sem desviar | parado, a cama é medida (11 cm, `obstaculo` 90%); andando a 150, o tecido só devolve eco perto demais para reagir | velocidade livre 150 → 120, segurança 20 → 25 cm e **regra de desvio garantido**: parado perto por 0,8 s = ré + giro mesmo sem o modelo confirmar ⏳ *gravar na ESP32 e testar de novo* |
| Voz parou de falar | bug do Chrome: `speak()` logo após `cancel()` é descartado | pausa de 150 ms antes de falar + botão **Testar voz** |
| Voz continuava depois de parar o carrinho | análise do LLM levava ~6 s e o navegador enfileirava os alertas | modelo *lite* (~1,1 s), voz cala ao parar, alertas atrasados não são falados |
| Upload da R3 exigiria soltar o cabo da câmera | a ESP32 transmite na mesma linha serial | a ESP32 fica em modo de gravação (silenciosa) durante o upload da R3 |
