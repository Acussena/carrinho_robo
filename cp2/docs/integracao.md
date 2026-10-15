# Integração do modelo no carrinho

## Visão geral

```
HC-SR04 ──► R3 (amostra a 20 Hz) ── "millis,cm" ──► ESP32-S3 do módulo de câmera
             ▲  motores (TB6612), controle IR        janela de 1 s → modelo TinyML
             └── {"N":110,"D1":dir,"D2":vel,"D3":classe} ◄──┘
```

O carrinho é o kit LAFVIN R3 com câmera. As duas placas já vêm ligadas pelo cabo "Camera UART" do shield (Serial 9600), e nenhum fio novo foi necessário:

| Placa | Firmware | Papel |
|---|---|---|
| R3 (ATmega328P) | [`r3/carrinho_r3`](../r3/carrinho_r3) | lê o HC-SR04 a 20 Hz e envia cada leitura; executa a ação do modelo nos motores; controle IR |
| ESP32-S3 (módulo de câmera) | [`esp32/carrinho_tinyml`](../esp32/carrinho_tinyml) | recebe as leituras, monta a janela, **roda o modelo** e decide a ação |

## R3: o que mudou em relação ao firmware da LAFVIN

O firmware de fábrica da LAFVIN é a base. O primeiro commit do CP2 é o código original, e as mudanças vêm nos commits seguintes, todas marcadas com `CP2` no código:

| Mudança | Arquivo |
|---|---|
| Amostragem do ultrassônico em intervalo fixo de 50 ms, sem o `delay(60)` do original (sem eco = 150 cm) | `MotorDriverSystem.cpp/.h` |
| Envio de cada leitura pela Serial: `millis,cm` | `RobotController.cpp` |
| Novo modo `OperationMode::TinyML` (tecla **0** do controle ou `{"N":101,"D1":4}`) | `RobotController.*`, `CommunicationInterface.cpp` |
| Comando `{"N":110,"D1":dir,"D2":vel,"D3":classe}`: ação do modelo, só aceita no modo TinyML | `CommunicationInterface.cpp` |
| Comando `{"N":111,"D1":1}`: liga/desliga o envio do sensor | `CommunicationInterface.cpp` |
| Segurança: no modo TinyML, para os motores se a ESP32 ficar 1 s sem mandar ação | `RobotController.cpp` |
| LED do shield com a cor da classe prevista | `CommunicationInterface.cpp` |

## ESP32: inferência

[`carrinho_tinyml.ino`](../esp32/carrinho_tinyml/carrinho_tinyml.ino) inclui a biblioteca exportada do Edge Impulse:

```cpp
#include <cp2_tinyml_inferencing.h>        // edge_impulse/biblioteca/ei-cp2_tinyml-arduino-1.0.1.zip
const int N_JANELA = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;   // 20 leituras = 1 s a 20 Hz
```

1. Cada linha `millis,cm` vinda da R3 entra numa janela circular de 20 leituras.
2. A cada 250 ms (o mesmo passo usado no treino), a janela é copiada em ordem cronológica e passada ao modelo:

```cpp
signal_t sinal;
numpy::signal_from_buffer(features, N_JANELA, &sinal);
run_classifier(&sinal, &resultado, false);
```

3. A classe com maior probabilidade é usada se passar de **60% de confiança**; abaixo disso fica `incerto` e a ação anterior é mantida.
4. A escala 1/150 do bloco Raw Data é aplicada dentro da biblioteca, então a ESP32 passa os centímetros crus, como no treino.

## Saída do modelo → ação no carrinho

| Classe prevista | Comando enviado à R3 | O que o carrinho faz | LED |
|---|---|---|---|
| `livre` | `{"N":110,"D1":1,"D2":170,"D3":0}` | frente rápido | verde |
| `aproximando` | `{"N":110,"D1":1,"D2":90,"D3":1}` | frente devagar | laranja |
| `obstaculo` | ré 400 ms (`D1:2`), giro à direita 450 ms (`D1:4`), parar (`D1:9`) | desvia | vermelho |
| `incerto` | mantém o último comando | — | — |

- **Sem bloqueio:** a manobra de desvio não trava o loop. Depois dela a janela é descartada, para o modelo não reagir de novo ao obstáculo antigo.
- **Comando repetido:** a ESP32 repete o comando a cada 100 ms, o que mantém a R3 dentro do timeout de 1 s.
- **Segurança:** abaixo de 6 cm a ESP32 manda parar, independente do modelo.

## Como usar

1. Gravar `r3/carrinho_r3` na R3 (`arduino:avr:uno`, com o cabo "Camera UART" desconectado durante a gravação) e `esp32/carrinho_tinyml` na ESP32-S3 do módulo de câmera (ESP32S3 Dev Module, Flash 8MB QIO, PSRAM OPI, core esp32 2.0.14).
2. Ligar o carrinho e apertar **0** no controle IR: modo TinyML.
3. **OK** sai do modo TinyML. As outras teclas continuam com as funções de fábrica.

A USB da ESP32 (115200) mostra as leituras e cada inferência:

```
# aproximando=0.00 livre=1.00 obstaculo=0.00 (0 ms) -> livre
# aproximando=1.00 livre=0.00 obstaculo=0.00 (0 ms) -> aproximando
# aproximando=0.29 livre=0.04 obstaculo=0.67 (0 ms) -> obstaculo
```

## Testes realizados

| Teste | Resultado |
|---|---|
| Envio do sensor R3 → ESP32 | ✅ 20 leituras/s, intervalo médio de 50,5 ms |
| Troca de modo e timeout de segurança na R3 | ✅ `Mode changed: 0 -> 6`, motores parados sem comando |
| Comandos ESP32 → R3 → motores (rodas suspensas, antes do modelo) | ✅ rodas giraram por 3 s e pararam no comando de standby |
| Modelo do Edge Impulse rodando na ESP32 | ✅ firmware de 342 KB, inferência em menos de 1 ms, classificações em tempo real (log acima) |
| Carrinho no chão, comandado pelo modelo | ⏳ a fazer: registrar aqui o resultado e o vídeo |
