# CP2 — TinyML no carrinho-robô (Edge Impulse + ESP32)

Evolução do carrinho do CP1: o sensor ultrassônico do carrinho gera um dataset, um modelo treinado no Edge Impulse roda **embarcado na ESP32-S3** e a classificação decide como o carrinho se move.

**COLETAR → TREINAR → TESTAR → EMBARCAR → INFERIR → AGIR**

Sensor → Coleta → CSV → Edge Impulse → Treinamento → Inferência → Biblioteca → ESP32 → Ação no carrinho

## Resultados

| Etapa | Resultado | Detalhes |
|---|---|---|
| Coleta | 66 amostras CSV (1565 leituras reais do HC-SR04, 78 s) + 2 gravações brutas de 60 s | [docs/coleta.md](docs/coleta.md) |
| Edge Impulse | Raw Data (janela 1 s, 20 Hz) → rede neural 20-20-10-3 | [docs/edge_impulse.md](docs/edge_impulse.md) |
| Treino (validação) | **94,1%** de acurácia (iteração 1: 64,7%) | [docs/edge_impulse.md](docs/edge_impulse.md#5-modelo-e-treinamento) |
| Teste (dados nunca vistos) | **96,55%**; `obstaculo` e `livre` 100% | [docs/edge_impulse.md](docs/edge_impulse.md#6-teste-de-inferência-dados-nunca-vistos) |
| No ESP32 | float32, inferência em menos de 1 ms, 1,4 KB de RAM | [docs/integracao.md](docs/integracao.md) |

## Arquitetura

Carrinho do kit **LAFVIN R3 Smart Robot Car com câmera**: placa R3 (UNO) + shield com driver TB6612, sensor ultrassônico HC-SR04, receptor IR e o **módulo de câmera com ESP32-S3**, ligado à R3 pelo cabo "Camera UART" do shield.

```
HC-SR04 ──► R3 (amostra a 20 Hz) ── "millis,cm" ──► ESP32-S3 do módulo de câmera
             ▲  motores, controle IR                 janela de 1 s → modelo TinyML
             └──── {"N":110,"D1":dir,"D2":vel,"D3":classe} ◄───┘
```

Nenhum fio novo: tudo passa pelo cabo original entre o shield e o módulo de câmera (Serial 9600).

| Classe prevista | Ação no carrinho | LED do shield |
|---|---|---|
| `livre` | frente rápido | verde |
| `aproximando` | frente devagar | laranja |
| `obstaculo` | ré e gira (desvia) | vermelho |

A menos de 20 cm o carrinho não avança mais (segurança); o modelo continua rodando e decide a ré ao reconhecer `obstaculo`. Se a ESP32 ficar 1 s sem mandar ação, a R3 para sozinha.

### Controle remoto IR

Os comandos de fábrica continuam iguais (setas, OK, 1 seguidor de linha, 2 desvio de obstáculo, 3 seguir objeto, 4-6 servo, 7-9 velocidade). O CP2 acrescentou:

| Tecla | Ação |
|---|---|
| **0** | modo **TinyML**: o modelo dirige (LED ciano até chegar a primeira inferência). Não use as setas: elas passam para o modo manual |
| OK | sai do modo TinyML (standby) |

## Estrutura

| Pasta | Conteúdo |
|---|---|
| [`r3/carrinho_r3/`](r3/carrinho_r3) | firmware da R3: código original da LAFVIN + modo TinyML, envio do sensor (comando 111) e ações do modelo (comando 110). Mudanças marcadas com `CP2` |
| [`esp32/coleta_dados/`](esp32/coleta_dados) | ESP32 da câmera: liga o envio da R3 e repassa `millis,cm` ao PC |
| [`esp32/carrinho_tinyml/`](esp32/carrinho_tinyml) | ESP32 da câmera: **firmware final com a inferência TinyML** |
| [`dados/csv/<classe>/`](dados/csv) | dataset: 66 CSVs rotulados (um arquivo por amostra) |
| [`dados/brutos/`](dados/brutos) | gravações contínuas de onde saíram parte das amostras |
| [`dados/descartado/`](dados/descartado) | coleta descartada (documentada em [docs/coleta.md](docs/coleta.md)) |
| [`scripts/`](scripts) | coleta Serial → CSV (manual, guiada e contínua) e console de testes |
| [`edge_impulse/prints/`](edge_impulse/prints) | evidências: upload, divisão, Impulse, features, treinos, matriz de confusão, teste, deploy |
| [`edge_impulse/biblioteca/`](edge_impulse/biblioteca) | biblioteca Arduino exportada do Edge Impulse (`cp2_tinyml_inferencing`) |
| [`docs/`](docs) | [coleta](docs/coleta.md) · [Edge Impulse](docs/edge_impulse.md) · [integração com o ESP32](docs/integracao.md) |

## Protocolo R3 ↔ ESP32 (adicionado no CP2)

| Comando | Sentido | Efeito |
|---|---|---|
| `{"N":111,"D1":1}` | ESP32 → R3 | liga o envio do ultrassônico: uma linha `millis,cm` a cada 50 ms (sem eco = 150) |
| `{"N":110,"D1":dir,"D2":vel,"D3":classe}` | ESP32 → R3 | ação do modelo. dir: 1 frente, 2 ré, 3 esquerda, 4 direita, 9 parar · vel 0-255 · classe 0/1/2 (cor do LED). Só tem efeito no modo TinyML |
| `{"N":101,"D1":4}` | PC/ESP32 → R3 | entra no modo TinyML (o mesmo que a tecla 0) |

## Como gravar

| Placa | Configuração na Arduino IDE / arduino-cli | Bibliotecas |
|---|---|---|
| R3 | Arduino Uno (`arduino:avr:uno`) | FastLED 3.5.0, Servo |
| ESP32-S3 da câmera | ESP32S3 Dev Module · Flash 8MB QIO · PSRAM OPI · "8M with spiffs" · **core esp32 2.0.14** | `edge_impulse/biblioteca/ei-cp2_tinyml-arduino-1.0.1.zip` (*Sketch → Include Library → Add .ZIP Library*) |

> - Para gravar a R3 pela USB, desconecte o cabo "Camera UART" (o módulo usa o mesmo RX).
> - Gravar a ESP32 da câmera substitui o firmware de vídeo de fábrica da LAFVIN, que fica disponível na documentação do kit.
> - Use o core esp32 2.0.x; as bibliotecas do Edge Impulse foram testadas nele.

Com o arduino-cli (troque `COMx` pelas portas de cada placa):

```bash
arduino-cli compile -u -b arduino:avr:uno -p COMx r3/carrinho_r3
```

```bash
arduino-cli lib install --zip-path edge_impulse/biblioteca/ei-cp2_tinyml-arduino-1.0.1.zip
```

```bash
arduino-cli compile -u -b esp32:esp32:esp32s3:FlashMode=qio,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB -p COMx esp32/carrinho_tinyml
```

## Como coletar dados

Com Python 3 e `pip install -r scripts/requirements.txt`, com a ESP32 da câmera na USB:

- `COLETAR.bat`: coleta guiada por classe, com Enter a cada amostra e conferência automática.
- `COLETAR_RAPIDO.bat`: grava 60 s guiados por fases e recorta as amostras sozinho.
- `scripts/coleta_serial.py --porta COMx --label livre --colunas distancia`: coleta manual de uma classe.

Os arquivos saem em `dados/csv/<classe>/` no formato do Edge Impulse (`timestamp,distancia`, rótulo no nome do arquivo).
