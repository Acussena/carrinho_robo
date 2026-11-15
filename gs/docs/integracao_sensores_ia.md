# Integração sensores ↔ IA

A IA funciona em três camadas. Cada uma roda onde faz mais sentido:

| Camada | Onde roda | Entrada | Saída | Precisa de internet? |
|---|---|---|---|---|
| 1. Segurança reativa | R3 | sensor de piso a 20 Hz | recusa avançar sobre desnível; para se a ESP32 calar | não |
| 2. Percepção e decisão (TinyML + regras) | ESP32-S3 | janela de 1 s do ultrassônico, flag de desnível, bateria | movimento, manobras, modo cauteloso, ocorrências | não |
| 3. Interpretação (LLM) | PC (painel) | ocorrência + contexto (sensores, histórico, local, hora) | gravidade, alerta falado, explicação, recomendação, respostas, relatório | sim (com regras offline de reserva) |

O robô **nunca** depende do PC para se mover com segurança. O painel acrescenta a interpretação e a comunicação com as pessoas.

## 1. Sensor adicional: módulo seguidor de linha como sensor de piso

- Três pares emissor/receptor IR voltados para o chão, na frente do carrinho, ligados em A2 (esquerda), A1 (centro) e A0 (direita).
- No CP2 e no firmware de fábrica, eles eram lidos com `digitalRead` só para seguir uma linha preta. Na GS são lidos com **`analogRead` (0–1023)** a cada amostra de 50 ms, junto com o ultrassônico:

| Situação | IR refletido | Valor lido |
|---|---|---|
| piso comum (medido no chão do teste) | muito | **0 / 0 / 0** |
| borda, degrau, frente levantada 10 cm (medido) | nada | **~735–750** |

Limiar padrão: **550**, no meio das duas leituras com folga. Uma superfície mais escura medida no teste (mesa) leu ~405, abaixo do limiar. **Vidro transparente** lê como "sem piso" (~730), porque a luz atravessa: o carrinho para e não avança (falha segura). Por isso, os testes em mesa de vidro usam um pano por cima.

- **Regra de desnível** (`MotorDriverSystem::updateFloorSensors`): os sensores são lidos a **cada 4 ms (250 Hz)**, separados da amostra de 20 Hz do ultrassom. Basta **1 dos 3** sensores acima do limiar (padrão 550) por **3 leituras seguidas (12 ms)**, porque o primeiro sensor a passar da borda já indica risco de queda. As 3 leituras filtram ruído elétrico.
- **Por que a reação precisa ser rápida:** o sensor aponta para baixo e só "vê" a borda quando já está sobre ela. Na primeira versão, que pedia 2 sensores e 2 amostras de 50 ms, a detecção levava até ~150 ms, e o carrinho ainda deslizava depois de desligar os motores. Isso deixava a frente para fora da mesa.

## 2. Protocolo R3 ↔ ESP32 (cabo Camera UART, 9600)

| Comando | Sentido | Efeito |
|---|---|---|
| `{"N":111,"D1":2}` | ESP32 → R3 | **GS:** envia `millis,cm,L,M,R,V10,desnivel,modo` a cada 50 ms (`D1:1` = formato do CP2, `D1:0` = desliga) |
| `{"N":112,"D1":550}` | ESP32 → R3 | **GS:** limiar do sensor de piso (100–1023) |
| `{"N":101,"D1":4}` / `{"N":101,"D1":0}` | ESP32 → R3 | entra no modo autônomo (= tecla 0) / **GS:** volta ao standby |
| `{"N":110,"D1":dir,"D2":vel,"D3":classe}` | ESP32 → R3 | ação; `D3` = cor do LED: 0 livre, 1 aproximando, 2 obstáculo, **3 desnível (roxo)** |

Exemplo de linha: `48213,37,212,198,240,78,0,6` = 48,2 s, 37 cm à frente, piso [212, 198, 240], bateria 7,8 V, sem desnível, modo 6 (patrulha autônoma).

- **modo**: com a R3 fora do modo autônomo (standby, manual), o modelo continua rodando e o painel continua recebendo os sensores, mas obstáculos e desníveis **não viram ocorrência**, porque o carrinho não está percorrendo o trajeto.
- **bateria < 5,5 V** = chave da bateria desligada e placa alimentada só pela USB: não gera alerta de bateria baixa, e o painel mostra "desligada (só USB)".

**Segurança dupla para o desnível:**
1. A **R3** guarda a última direção pedida. Se for para frente (1, 5 ou 7) e o desnível estiver confirmado, ela **freia na hora** com um pulso de ré de 150 ms (freio ativo, porque só desligar os motores deixa o carrinho deslizar), sem esperar a ESP32. Depois para e recusa novos comandos de frente.
2. A **ESP32** vê a flag e assume o controle com uma manobra: ré 700 ms → giro 600 ms. Depois descarta a janela do TinyML e volta a classificar.

## 3. ESP32: fusão TinyML + piso + bateria

Ordem de prioridade a cada volta do `loop()`:

1. **R3 sem dados há 2 s** → ocorrência `falha_r3`, não envia ação (a R3 para em 1 s).
2. **Desnível** → manobra de desnível (interrompe tudo, exceto a ré em andamento).
3. **Manobra em andamento** → continua sem bloquear o loop.
4. **Distância < 25 cm andando para frente** → para. Se continuar perto por **0,8 s** sem o TinyML confirmar `obstaculo` (tecido macio, objeto fora do dataset), a **regra de desvio garantido** faz ré + giro e registra a ocorrência com confiança 0 (= decidida pela regra).
5. **TinyML** a cada 250 ms sobre a janela de 20 leituras (modelo do CP2, 96,55% no teste). Abaixo de 60% de confiança vale `incerto` e a ação anterior é mantida.

**Adaptação de comportamento (autonomia):**

| Gatilho | Mudança |
|---|---|
| 2 ocorrências (`obstaculo`/`desnivel`) em 30 s | `cautela = true` por 30 s: velocidades 150/90 → 110/75; ocorrência `trecho_critico` |
| bateria < 7,0 V por 5 s | velocidades de economia; ocorrência `bateria_baixa` (rearma acima de 7,2 V) |
| R3 manda só `millis,cm` (firmware antigo) | pede o formato da GS a cada 3 s; após 6 s, ocorrência `sensor_piso_indisponivel` e segue só com o ultrassônico |

**Ocorrências** ficam num buffer circular de 40 itens (id, hora, tipo, distância, piso, bateria, confiança) e saem por:
- USB: linha `@E {json}` na hora, mais telemetria `@T {json}` a cada 500 ms;
- Wi-Fi: `GET /status`, `GET /eventos?desde=<id>`, `GET /cmd?acao=iniciar|parar|pausar|retomar|limiar&valor=N`.

O Wi-Fi conecta **sem bloquear**: se a rede cair, o robô segue autônomo e reconecta sozinho.

## 4. Painel: LLM (Gemini) sobre os dados dos sensores

Para cada ocorrência nova, o painel monta um prompt com:
- o **local** da patrulha e o **horário**;
- o **tipo** da ocorrência e o que ele significa em termos de sensores;
- as **leituras no instante**: distância, piso [E, C, D], limiar, bateria, confiança do modelo;
- o **histórico** da patrulha (contagem por tipo).

O modelo responde em **JSON** (`responseMimeType: application/json`):

```json
{
  "severidade": "alta",
  "categoria": "desnível sem rampa",
  "alerta_voz": "Atenção: desnível no piso à frente, possível degrau sem rampa. Cadeirantes devem buscar outro acesso.",
  "explicacao": "Dois dos três sensores de piso passaram de 900, indicando ausência de piso logo à frente.",
  "recomendacao": "Instalar rampa conforme NBR 9050 ou sinalizar o desnível com piso tátil de alerta."
}
```

- O **alerta_voz** é falado pelo navegador (Web Speech API, voz pt-BR) e fica na lista de alertas.
- **Desnível tem aviso imediato:** assim que a ocorrência chega, o painel fala "Cuidado! Desnível à frente!", cortando qualquer outra frase, sem esperar o LLM. A análise do LLM vem 1 a 2 s depois, com a gravidade e a recomendação. O painel consulta o robô 5 vezes por segundo e o navegador consulta o painel 4 vezes por segundo, para o aviso sair em menos de meio segundo.
- **A voz acompanha o robô:** os alertas em tempo real usam um modelo *lite* (`gemini-3.5-flash-lite`, ~1,1 s por análise); relatório e perguntas usam o `gemini-3.5-flash`. Ao parar a patrulha (botão Parar ou tecla OK), a voz se cala na hora. Alertas com mais de 10 s de atraso, ou que chegam com o robô parado, ficam só na tela e no log. A frase em curso nunca é cortada por outro alerta: os que chegam enquanto ela é dita esperam, e ao final só o mais recente é falado. O fim da fala é detectado pelo estado do sintetizador, porque o navegador às vezes não dispara o evento de término.
- Severidade, explicação e recomendação vão para o cartão da ocorrência e para o **CSV** (`gs/painel/saida/ocorrencias_*.csv`).
- **Pergunte à IA:** perguntas livres ("esta calçada é acessível?") são respondidas com base nos dados da patrulha.
- **Relatório:** o LLM gera um relatório Markdown (resumo, tabela por gravidade, pontos críticos, recomendações priorizadas), salvo em `gs/painel/saida/relatorio_*.md`.

**Robustez:**
- sem `GEMINI_API_KEY`, sem internet, cota excedida (HTTP 429) ou modelo descontinuado (HTTP 404, com troca automática para o próximo da lista) → **regras offline** com a mesma estrutura (`fonte: "regras (offline)"`);
- a mesma ocorrência repetida em menos de 8 s reaproveita a análise anterior, para poupar a cota gratuita;
- sem dados do robô por 3 s → o painel avisa "Sem dados do robô" (o robô continua autônomo).
