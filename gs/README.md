# GS — Fiscal de Calçadas: robô CitizenTech de acessibilidade urbana

Evolução do carrinho do CP2 (TinyML com o ultrassônico) para ajudar a **identificar barreiras de acessibilidade em calçadas, corredores e prédios públicos**. O robô percorre o trajeto sozinho e detecta **obstáculos** e **desníveis** (buraco, degrau, meio-fio sem rampa). Ele **se adapta** ao que encontra, e uma **IA generativa** transforma as leituras em **alertas falados e num relatório para a prefeitura**.

🎥 **Vídeo de demonstração:** _(colar aqui o link do YouTube/Drive)_

## 1. O problema (CitizenTech)

- Calçadas com buracos, degraus e meio-fios sem rampa são a principal barreira de mobilidade para **pessoas com deficiência, idosos e quem empurra carrinho de bebê**. A NBR 9050 exige uma faixa livre e sem desníveis.
- O mapeamento dessas barreiras ainda é feito sobretudo por vistoria manual ou por reclamação de cidadãos, o que é lento e incompleto.
- **Proposta:** um robô de baixo custo (o kit do curso, sem peças novas) que percorre o trajeto e:
  1. **percebe** obstáculos (ultrassônico + TinyML) e desníveis no piso (**sensor adicional da GS**);
  2. **decide** sozinho o que fazer: desviar, recuar da borda, andar com cautela num trecho ruim, economizar bateria;
  3. **interpreta e comunica**: um LLM classifica a gravidade de cada ocorrência, avisa por voz quem está no local e gera um relatório com recomendações para a gestão pública.

O mesmo uso vale para o campo: trilhas e acessos de escolas rurais, ou postos de saúde com piso irregular.

## 2. O que mudou em relação ao CP2

| | CP2 | GS |
|---|---|---|
| Sensores usados | ultrassônico HC-SR04 | ultrassônico **+ sensor de piso (módulo seguidor de linha, 3 canais IR)** + tensão da bateria |
| IA | TinyML: livre / aproximando / obstáculo | TinyML **+ detecção de desnível + LLM (Gemini)** para alerta, gravidade, recomendação, perguntas e relatório |
| Decisão | anda, desacelera, desvia | + **para e recua da borda**, **modo cauteloso** automático em trecho crítico, **economia** com bateria baixa, recusa andar se for levantado do chão |
| Saída para o cidadão | LED | LED + **voz** + **painel web** + **relatório** e **log CSV** |
| Falhas tratadas | timeout da ESP32 | + R3 sem dados, sensor de piso indisponível, Wi-Fi caído, LLM sem internet ou sem cota |

> **Sensor adicional:** o módulo seguidor de linha já vinha no kit, mas nunca foi usado pelo robô inteligente do CP2. Na GS ele foi incorporado com uma **função nova**: em vez de seguir uma linha, ele é lido em **analógico como sensor de piso**. Sobre um piso, o IR volta (valor baixo); sobre um buraco, degrau ou borda, nada volta (valor alto). Nenhuma peça foi comprada.

## 3. Arquitetura

```
 ┌──────────────── Carrinho (autônomo, não depende do PC) ────────────────┐
 │                                                                         │
 │  HC-SR04 ─┐                                                             │
 │  Piso IR ─┼─► R3 (UNO) 20 Hz ── "millis,cm,L,M,R,V10,desnível,modo" ──►  ESP32-S3
 │  Bateria ─┘    │  motores TB6612, IR, LED          (cabo Camera UART)  │ TinyML (obstáculo)
 │                │  segurança local: não avança                          │ detecção de desnível
 │                │  sobre desnível; para sem ESP32                       │ decisão autônoma
 │                └──◄── {"N":110,"D1":dir,"D2":vel,"D3":classe} ◄────────┤ registro de ocorrências
 └─────────────────────────────────────────────────────────────────────────┼──────────────────────┘
                                                                           │ Wi-Fi (HTTP /status /eventos /cmd)
                                                                           │ ou USB (@T / @E)
                                                                ┌──────────▼─────────────────────┐
                                                                │ Painel no PC (gs/painel)        │
                                                                │  LLM Gemini: gravidade, alerta, │
                                                                │  recomendação, perguntas,       │
                                                                │  relatório · voz pt-BR · CSV    │
                                                                └─────────────────────────────────┘
```

Detalhes da integração sensores ↔ IA: **[docs/integracao_sensores_ia.md](docs/integracao_sensores_ia.md)**

### Comportamento autônomo

| Situação detectada | Quem detecta | O que o robô faz | LED | Ocorrência |
|---|---|---|---|---|
| Caminho livre | TinyML (ultrassônico) | frente rápido (120; 100 em cautela) | verde | — |
| Algo se aproximando | TinyML | frente devagar (90; 75 em cautela) | laranja | — |
| Objeto parado perto | TinyML (ou regra: parado a < 25 cm por 0,8 s) | ré + giro | vermelho | `obstaculo` |
| **Sem piso à frente** (buraco, degrau, borda) | **sensor de piso** (R3) | **para na hora (R3), ESP32 faz parada → ré → giro** | **roxo** | `desnivel` |
| 2 barreiras em 30 s | ESP32 | **modo cauteloso** por 30 s (velocidades menores) | — | `trecho_critico` |
| Bateria < 7,0 V por 5 s | ESP32 | velocidades de economia | — | `bateria_baixa` |
| Robô levantado do chão | sensor de piso | recusa andar para frente | roxo | `desnivel` |
| R3 sem dados por 2 s | ESP32 | não age às cegas; a R3 para sozinha em 1 s | — | `falha_r3` |
| R3 com firmware antigo | ESP32 | segue só com o ultrassônico | — | `sensor_piso_indisponivel` |
| Wi-Fi caiu / LLM fora | painel | robô continua igual; IA usa regras offline | — | aviso no painel |

## 4. Componentes

Tudo do kit **LAFVIN R3 Smart Robot Car com câmera** (R$ 389,00, ver [planilha de custos](../organizacao/especificacoes_carrinho_arduino.md)). **Nenhuma peça nova.**

| Componente | Função na GS | Ligação |
|---|---|---|
| Placa R3 (ATmega328P, compatível com Arduino UNO) | lê os sensores a 20 Hz, aciona os motores, segurança local | — |
| Shield com driver TB6612 + 4 motores DC | tração | PWM 5/6, DIR 7/8, STBY 3 |
| Sensor ultrassônico HC-SR04 | distância à frente → TinyML | TRIG 13, ECHO 12 |
| **Módulo seguidor de linha (3× IR refletivo)** | **sensor de piso / desnível (sensor adicional da GS)** | **A2 (esq.), A1 (centro), A0 (dir.)**, já montado embaixo da frente do carrinho |
| Divisor de tensão da bateria | nível da bateria | A3 |
| LED RGB do shield | cor da decisão | pino 4 (FastLED) |
| Receptor IR + controle | tecla 0 = patrulha, OK = parar | pino 9 |
| Módulo ESP32-S3 (câmera) | TinyML, decisão, Wi-Fi, API do painel | cabo "Camera UART" (RX 3 / TX 40, 9600) |
| 2 baterias 18650 | alimentação | — |
| PC ou notebook (qualquer) | painel com IA e voz | Wi-Fi ou USB-C da ESP32 |

## 5. Estrutura

| Pasta | Conteúdo |
|---|---|
| [`r3/carrinho_r3/`](r3/carrinho_r3) | firmware da R3: base do CP2 + sensor de piso, bateria, segurança de desnível, comandos 101/111/112. Mudanças marcadas com `GS` |
| [`esp32/carrinho_gs/`](esp32/carrinho_gs) | firmware da ESP32-S3: TinyML do CP2 + desnível + autonomia + ocorrências + Wi-Fi/API |
| [`painel/`](painel) | painel web com IA (Python, só biblioteca padrão; `pyserial` para USB) |
| [`docs/`](docs) | [integração sensores ↔ IA](docs/integracao_sensores_ia.md) · [testes](docs/testes.md) · [roteiro do vídeo](docs/roteiro_video.md) |
| [`../cp2/`](../cp2) | dataset, treino no Edge Impulse e biblioteca do modelo TinyML (reaproveitados) |

## 6. Como instalar e executar

### 6.1 Gravar a R3

Arduino IDE → placa **Arduino Uno**, bibliotecas **FastLED 3.5.0** e **Servo** (no arduino-cli: `arduino-cli lib install FastLED@3.5.0 Servo`). Durante a gravação, a ESP32 não pode transmitir na mesma linha: desconecte o cabo "Camera UART" ou deixe a ESP32 em modo de gravação, sem soltar nada:

```bash
esptool --chip esp32s3 -p COMy --after no_reset chip_id
```

Grave a R3 e depois reinicie a ESP32 com `esptool --chip esp32s3 -p COMy --after hard_reset chip_id`.

```bash
arduino-cli compile -u -b arduino:avr:uno -p COMx gs/r3/carrinho_r3
```

### 6.2 Gravar a ESP32-S3

1. Instale a biblioteca do modelo: *Sketch → Include Library → Add .ZIP Library* → `cp2/edge_impulse/biblioteca/ei-cp2_tinyml-arduino-1.0.1.zip`.
2. Wi-Fi (opcional, para o painel sem cabo): copie `gs/esp32/carrinho_gs/config_wifi.example.h` para `config_wifi.h` e preencha a rede. O hotspot do celular funciona; o PC precisa estar na mesma rede.
3. Placa **ESP32S3 Dev Module**, Flash 8MB QIO, PSRAM OPI, partição "8M with spiffs", **core esp32 2.0.14**.

```bash
arduino-cli compile -u -b esp32:esp32:esp32s3:FlashMode=qio,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB -p COMx gs/esp32/carrinho_gs
```

Com a ESP32 no USB, o Monitor Serial (115200) mostra o IP: `# wifi conectado: http://192.168.x.x`.

### 6.3 Painel com IA

1. Python 3.9+ no PC.
2. **Chave do Gemini (gratuita, sem cartão):** entre em <https://aistudio.google.com/apikey>, crie a chave, copie `gs/painel/.env.example` para `gs/painel/.env` e cole em `GEMINI_API_KEY=`. Sem chave, o painel funciona com as regras offline.
3. Rode `gs/painel/PAINEL.bat` e escolha **1** (Wi-Fi, IP da ESP32), **2** (USB, porta COM) ou **3** (simulação). Ou direto:

```bash
python gs/painel/painel.py --ip 192.168.0.50 --local "Calçada da Av. Paulista, 1100"
```

4. Abra <http://localhost:8000>, deixe **voz** marcada e clique em **Iniciar patrulha** (ou aperte **0** no controle IR).
5. **Reiniciar painel** (botão no topo) zera ocorrências, alertas e contadores para uma nova patrulha ou gravação, sem perder a conexão com o robô. Cada reinício abre um novo CSV em `gs/painel/saida/`.

### 6.4 Calibrar o sensor de piso (1 minuto)

No painel, o cartão **Sensor de piso** mostra os 3 valores ao vivo e o limiar (linha vermelha).

1. Carrinho no chão: anote os valores (no nosso teste: 0 / 0 / 0).
2. Levante a frente do carrinho uns 10 cm (ou segure-a para fora da borda de uma mesa): anote os valores (no nosso teste: ~740).
3. Ajuste o limiar entre os dois (o padrão gravado é **550**, calibrado no nosso carrinho) e clique em **ajustar**. Isso vale até desligar; para fixar, mude `limiarPiso` em `carrinho_gs.ino`.

> **Segurança do teste:** faça o teste de borda numa mesa baixa, com alguém pronto para segurar o carrinho.

## 7. Testes

Plano e resultados: **[docs/testes.md](docs/testes.md)**. Cobre a leitura do sensor de piso, o desnível na mesa, obstáculo, trecho crítico, robô levantado, bateria, cabo solto, Wi-Fi caído, LLM sem internet e firmware antigo.

## 8. Limitações e próximos passos

- **Piso muito escuro** (asfalto molhado, faixa preta) reflete pouco IR e pode parecer "sem piso". O limiar ajustável e a regra de 2 dos 3 sensores reduzem o problema; um próximo passo é treinar um modelo TinyML com as leituras de piso.
- **Sem localização:** as ocorrências têm horário, mas não coordenadas. O celular que roda o painel poderia fornecer o GPS.
- **Câmera:** o módulo ESP32-S3 tem câmera OV3660. Enviar uma foto de cada ocorrência ao LLM multimodal é a evolução natural.
- O ultrassônico do CP2 tem as limitações já documentadas em [cp2/docs/integracao.md](../cp2/docs/integracao.md#limitações-conhecidas).
