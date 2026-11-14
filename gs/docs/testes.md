# Testes e demonstração

Legenda: ✅ passou · ⚠️ passou com ressalva · ❌ falhou · ⏳ a executar

> Preencha a coluna **Resultado** durante os testes. Se possível, salve o Monitor Serial e o CSV do painel em `gs/docs/logs/`.

## 1. Painel e IA sem o robô (simulação)

| # | Teste | Como | Esperado | Resultado |
|---|---|---|---|---|
| S1 | Painel em simulação, IA offline | `python gs/painel/painel.py --simular` sem `.env` | telemetria ao vivo; ocorrências de obstáculo, desnível e trecho crítico com análise `regras (offline)`; alertas falados | ✅ executado no desenvolvimento: 4 ocorrências analisadas, CSV gravado |
| S2 | Relatório offline | botão **Gerar relatório** | Markdown com tabela de ocorrências salvo em `saida/` | ✅ executado no desenvolvimento |
| S3 | Pergunta offline | "A calçada é acessível?" | resposta com a contagem de ocorrências | ✅ executado no desenvolvimento |
| S4 | Comandos | botões Parar / Iniciar | ocorrência `patrulha` e alerta "Patrulha encerrada/iniciada" | ✅ executado no desenvolvimento |
| S5 | IA com LLM | `.env` com `GEMINI_API_KEY` e simulação | ocorrências com `fonte: LLM gemini-…`, alerta contextualizado | ⏳ |
| S6 | LLM sem internet | desligar a internet com o painel aberto | o erro aparece no selo "IA"; as ocorrências seguem analisadas pelas regras offline | ⏳ |

## 2. Sensor adicional (piso) no robô

| # | Teste | Como | Esperado | Resultado |
|---|---|---|---|---|
| P1 | Leitura no chão | carrinho parado no piso, painel aberto | L/M/R baixos e estáveis (anotar: ___ / ___ / ___) | ⏳ |
| P2 | Leitura sem piso | levantar a frente do carrinho | L/M/R altos (anotar: ___ / ___ / ___); "DESNÍVEL À FRENTE" | ⏳ |
| P3 | Calibração | limiar entre P1 e P2 → **ajustar** | o desnível só acende sem piso | ⏳ |
| P4 | Faixa preta no chão | passar sobre fita isolante preta | ⚠️ esperado: pode detectar como desnível (limitação documentada); anotar | ⏳ |

## 3. Comportamento autônomo

| # | Teste | Como | Esperado | Resultado |
|---|---|---|---|---|
| A1 | **Borda da mesa = degrau/meio-fio** | mesa baixa, patrulha iniciada, carrinho indo em direção à borda (alguém pronto para segurar) | para antes de cair, LED roxo, ré + giro; alerta falado de desnível | ⏳ |
| A2 | Obstáculo (caixa/mochila) | caixa parada no caminho | desacelera (laranja), ré + giro (vermelho); alerta de obstáculo | ⏳ |
| A3 | Trecho crítico | caixa + borda em menos de 30 s | "modo cauteloso" no painel, velocidade menor; alerta de trecho crítico | ⏳ |
| A4 | Robô levantado | patrulha ativa, erguer o carrinho | rodas não giram para frente (segurança da R3) | ⏳ |
| A5 | Caminho livre | 1–2 m sem nada | frente rápido, LED verde | ⏳ |

## 4. Situações de falha

| # | Teste | Como | Esperado | Resultado |
|---|---|---|---|---|
| F1 | Cabo R3 ↔ ESP32 solto | desconectar o cabo "Camera UART" com a patrulha ativa | motores param em ≤ 1 s; ocorrência `falha_r3`; ao reconectar, `r3_reconectada` | ⏳ |
| F2 | Wi-Fi caído | desligar o hotspot | robô continua desviando sozinho; painel mostra "sem dados do robô"; ao religar, reconecta | ⏳ |
| F3 | R3 com firmware do CP2 | gravar `cp2/r3` e ligar | ocorrência `sensor_piso_indisponivel`; robô segue só com o ultrassônico | ⏳ (opcional) |
| F4 | Bateria fraca | rodar até < 7,0 V (ou observar a tensão no painel) | `bateria_baixa`, velocidade de economia | ⏳ (opcional) |
| F5 | Sem chave do LLM | apagar o `.env` | painel funciona com regras offline | ✅ (= S1) |

## 5. Evidências

- Logs do Monitor Serial (115200) da ESP32: `# ... -> desnivel`, linhas `@E`.
- CSV gerado pelo painel: `gs/painel/saida/ocorrencias_*.csv` (copie um para `gs/docs/logs/`).
- Relatório gerado pelo LLM: `gs/painel/saida/relatorio_*.md` (copie um para `gs/docs/logs/`).
- Vídeo: ver o roteiro em [roteiro_video.md](roteiro_video.md).
