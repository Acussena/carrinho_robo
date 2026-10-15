# Coleta de dados

## Sensor e dados

- **Sensor:** ultrassônico HC-SR04 do kit, na frente do carrinho (pinos D12/D13 da R3).
- **Grandeza:** distância até o obstáculo à frente, em cm (inteiro, 2–150; sem eco ou acima de 150 cm = 150).
- **Taxa:** 20 Hz (uma leitura a cada 50 ms), feita pela R3 em intervalo fixo e enviada à ESP32-S3 pelo cabo "Camera UART".
- **Quem grava:** a ESP32-S3 do módulo de câmera recebe cada leitura e repassa ao PC pela USB; os scripts em `scripts/` salvam os arquivos.

Todos os dados foram medidos pelo grupo, com o sensor do próprio carrinho, em 08/10/2026.

## Formato dos arquivos

Uma amostra por arquivo, em `dados/csv/<classe>/<classe>.<id>.csv`:

```csv
timestamp,distancia
0,87
50,85
101,83
```

`timestamp` em ms desde o início da amostra (relógio da R3). O nome `classe.<id>.csv` faz o Edge Impulse reconhecer o rótulo no upload.

## Classes

O modelo olha uma **janela de 1 s (20 leituras)** e decide como o carrinho deve se mover:

| Classe | Situação | Ação no carrinho |
|---|---|---|
| `livre` | nada perto à frente | frente rápido |
| `aproximando` | algo vindo na direção do carrinho, ou o carrinho indo em direção a uma parede | frente devagar |
| `obstaculo` | algo parado bem perto (≲ 30 cm) | ré e gira |

## Como a coleta foi feita

A coleta passou por três rodadas. A primeira foi descartada.

### 1. Coleta manual por amostra (`scripts/coleta_serial.py`, `COLETAR.bat`)

O script pede Enter, grava 2 s (≈ 40 leituras) e salva um arquivo. A versão guiada (`scripts/coleta_guiada.py`) mostra a distância ao vivo e confere se a amostra combina com a classe.

- A **primeira tentativa** de `livre` (30 amostras) foi **descartada**: as leituras ficaram entre 1 e 36 cm porque o carrinho estava virado para objetos próximos. Os arquivos estão em `dados/descartado/livre_tentativa1/`.
- A segunda tentativa gerou as **12 amostras de `livre`** usadas no dataset (70–150 cm).

### 2. Coleta contínua com recorte automático (`scripts/coleta_continua.py`, `COLETAR_RAPIDO.bat`)

Para coletar `aproximando` e `obstaculo` mais rápido, o sensor gravou 60 s sem parar enquanto uma pessoa movia uma caixa/mão na frente do carrinho parado. A gravação inteira foi guardada em `dados/brutos/` e depois recortada em janelas de 1 s (passo de 0,5 s).

Cada janela é conferida por regras sobre a distância. As regras usam filtro de mediana (5 leituras) para ignorar leituras soltas sem eco; o arquivo salvo mantém a leitura original do sensor, igual ao que o carrinho vê na inferência. Janelas que não se encaixam em nenhuma regra são descartadas.

| Regra | Classe |
|---|---|
| todas as leituras ≥ 60 cm | `livre` |
| todas as leituras ≤ 30 cm | `obstaculo` |
| distância cai ≥ 15 cm em 1 s, começando ≥ 30 cm, sem voltar a subir | `aproximando` |

| Sessão bruta | Leituras | Duração | Como foi conduzida |
|---|---|---|---|
| `sessao_20261008_214240.csv` | 1190 | 60 s | movimentos à vontade (caixa aproximando, segurando, afastando) — rótulo só pela regra |
| `sessao_20261008_214629.csv` | 1188 | 60 s | guiada por fases na tela (15 s obstáculo, 15 s espaço aberto, 30 s aproximar/afastar) — a fase define a classe e a regra só confere |

A fase "espaço aberto" da segunda sessão não gerou amostras de `livre`: o carrinho estava numa mesa com objetos a menos de 60 cm.

## Dataset final

| Classe | Arquivos | Leituras | Duração total | Faixa | Média |
|---|---|---|---|---|---|
| `livre` | 12 | 485 | 24,2 s | 70–150 cm | 118 cm |
| `aproximando` | 39 | 780 | 39,0 s | 5–150 cm | 45 cm |
| `obstaculo` | 15 | 300 | 15,0 s | 3–83 cm | 17 cm |
| **Total** | **66** | **1565** | **78 s** | | |

Mais 2378 leituras nas duas gravações contínuas brutas (`dados/brutos/`).

**Observações**

- O ultrassônico às vezes perde o eco (mão inclinada, superfície macia) e devolve 150 por uma leitura. Essas falhas ficaram nos dados de propósito, porque também acontecem durante a inferência.
- `obstaculo` tem menos dados. Se o modelo confundir essa classe, a próxima coleta deve focar nela.
- As amostras de `aproximando` e `obstaculo` recortadas com passo de 0,5 s se sobrepõem pela metade. Na divisão treino/teste, janelas vizinhas podem cair em lados diferentes, o que deixa a acurácia de teste um pouco otimista.
