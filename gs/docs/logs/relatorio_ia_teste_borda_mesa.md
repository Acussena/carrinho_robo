# Relatório Técnico: Patrulha de Acessibilidade

**Local:** Calçada de teste - FIAP  
**Início:** 14/11/2026 18:46  
**Duração:** 78.9 minutos  

---

## Resumo
O robô "Fiscal de Calçadas" concluiu a patrulha no local especificado mapeando barreiras de acessibilidade (afetando pessoas com deficiência, idosos e carrinhos de bebê) para subsidiar a manutenção urbana. Durante a operação, foram registradas 2 patrulhas, 4 desníveis, 7 obstáculos e 1 trecho crítico.

---

## Ocorrências por Gravidade

| Horário | Tipo | Severidade | Distância (cm) | Leituras do Piso (IR) | Tensão da Bateria (V) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 18:51:58 | desnível | alta | 150 | [734, 735, 735] | 7.8 |
| 18:52:00 | obstáculo | alta | 3 | [0, 0, 0] | 8.2 |
| 18:52:00 | trecho_critico | alta | 3 | [0, 0, 0] | 8.2 |
| 18:52:05 | obstáculo | média | 150 | [0, 0, 0] | 8.1 |
| 18:52:05 | desnível | alta | 112 | [0, 749, 748] | 8.2 |
| 18:52:10 | desnível | alta | 36 | [748, 748, 748] | 8.1 |
| 18:52:13 | obstáculo | média | 1 | [0, 0, 0] | 8.1 |
| 18:52:17 | desnível | alta | 6 | [747, 745, 748] | 8.1 |
| 18:52:20 | obstáculo | alta | 20 | [0, 0, 0] | 8.0 |
| 18:52:21 | obstáculo | alta | 31 | [0, 0, 0] | 8.0 |
| 18:52:23 | obstáculo | alta | 19 | [0, 0, 0] | 8.1 |
| 18:52:28 | obstáculo | alta | 17 | [0, 0, 0] | 8.1 |

---

## Pontos Críticos
* Identificado **1 trecho crítico** às 18:52:00, associado à presença simultânea de obstáculo a curta distância (3 cm) e leituras críticas de piso.
* Alta incidência de desníveis com valores analógicos de infravermelho acima do limiar (registros entre 734 e 749, indicando ausência de piso/buracos/degraus/meio-fio).
* Concentração de múltiplos obstáculos físicos detectados pelo sensor ultrassônico e classificados pelo modelo TinyML com severidade alta em curtas distâncias.

---

## Recomendações Priorizadas
1. **Intervenção imediata no trecho crítico** (18:52:00): Realizar inspeção física para remoção de barreiras e correção de falhas estruturais no pavimento.
2. **Correção de desníveis**: Executar nivelamento e reparos nos pontos onde o sensor de infravermelho detectou ausência de piso (valores acima de 550), garantindo segurança para pedestres, idosos e carrinhos de bebê.
3. **Remoção de obstáculos**: Notificar responsáveis sobre os objetos/obstruções encontrados nas distâncias mais curtas (entre 1 cm e 31 cm) mapeados ao longo da patrulha.

---

## Estado do Robô
* **Uptime:** 4.764.558 ms
* **Status do Sensor R3 (Infravermelho):** Operacional (`r3_ok: true`)
* **Distância Atual (Ultrassônico):** 19 cm
* **Estado do Piso:** Disponível, sem desnível atual (Leituras L: 0, M: 0, R: 0; Limiar: 550)
* **Status Operacional:** Patrulhando: Não (Pausado: Não, Cautela: Não)
* **Tensão da Bateria:** 7.9 V (Ligada: Sim, Bateria baixa: Não)
* **Modelo TinyML:** Classe "obstaculo" (Confiança: 0.8 / 80%)
* **Decisão Atual:** Manobra de ré (`manobra: re`, Direção: 2, Velocidade: 150)
* **Contagem Acumulada do Robô:** 
  * Obstáculos: 7
  * Desníveis: 4
  * Trechos críticos: 1
  * Bateria baixa: 0
  * Falhas no R3: 0
* **Conectividade:** Wi-Fi RSSI de -61 dBm

---
_Gerado pelo painel do Fiscal de Calçadas em 14/11/2026 20:05 (LLM gemini-3.5-flash-lite)._
