# carrinho_robo

# Documentação Técnica — Chassi do Robô

**Disciplina/Projeto:** Project-based Maker Lab
**Integrantes:**
- Açussena Macedo Mautone - 552568
- Felipe Heilmann Marques - 551026
- Felipe Voidela Toledo - 98595
- Carlos Eduardo Caramante Ribeiro - 552159
- Ian Cancian Nachtergaele - 98387

---

## 1. Ficha de Requisitos

### 1.1 Dimensões gerais do chassi

Com base no modelo 3D (`chassi_robo_v3.stl`), o chassi possui estrutura de **dois andares**, unidos por colunas nos quatro cantos:

| Eixo | Medida |
|---|---|
| Comprimento (Y) | 200 mm |
| Largura (X) | 150 mm |
| Altura total (Z) | 59 mm |

- **Andar inferior** (~0 a 8 mm de altura): base onde ficam fixados os motores e as rodas.
- **Andar superior** (~40 a 59 mm de altura): plataforma para a placa controladora, ponte H e demais componentes eletrônicos, com um recorte retangular central (passagem de fiação/ventilação).
- **Colunas de sustentação**: 4 colunas nos cantos conectando os dois andares.

### 1.2 Quantidade de motores

**2 motores DC** (motor esquerdo e motor direito), configuração de tração diferencial.

### 1.3 Placa controladora

**ESP32** (conectividade Wi-Fi/Bluetooth integrada, permite controle remoto do robô).

### 1.4 Posição dos componentes

| Componente | Posição no chassi |
|---|---|
| Motor esquerdo | Andar inferior, lateral esquerda |
| Motor direito | Andar inferior, lateral direita |
| Rodas | Acopladas aos motores, nas laterais do andar inferior |
| ESP32 | Andar superior, região central |
| Ponte H | Andar superior, próxima ao ESP32 (facilita conexão com os motores) |
| Bateria 18650 | Andar inferior ou superior, região central (centralizar o peso) |
| Sensor HC-SR04 | Andar inferior, parte frontal (furo central identificado no modelo, voltado para a frente do robô) |

### 1.5 Carenagem (cobertura)

O andar superior funciona como **cobertura parcial** do robô, protegendo a fiação e os componentes eletrônicos do andar inferior. Possui um recorte retangular central para passagem de cabos e dissipação de calor. Não há cobertura lateral fechada — as laterais permanecem abertas para acesso aos motores e rodas.

---

## 2. Croqui do Chassi

O croqui foi modelado em 3D e está disponível no arquivo [`chassi_robo_v3.stl`](./chassi_robo_v3.stl) deste repositório.

Vistas geradas a partir do modelo:

![Vistas do chassi](./chassi_views.png)

*Da esquerda para a direita: vista 3D isométrica, vista superior (top view), vista frontal (front view).*

### 2.1 Observação

Dimensões acima são teóricas, pois usaremos um carrinho pronto!

---

## 3. Tabela Dimensional dos Componentes

| Componente | Comprimento (mm) | Largura (mm) | Altura (mm) | Forma de fixação |
|---|---|---|---|---|
| Motor esquerdo | 70 | 37 | 23 | _(preencher)_ |
| Motor direito | 70 | 37 | 23 | _(preencher)_ |
| ESP32 | 49 | 25,5 | 7 | _(preencher)_ |
| Ponte H | 43 | 43 | 27 | _(preencher)_ |
| Bateria 18650 | 67 | 18 | 18* | _(preencher)_ |
| Sensor HC-SR04 | 45 | 20 | 15 | _(preencher)_ |

\* A bateria 18650 é cilíndrica (diâmetro 18 mm), por isso largura e altura coincidem quando ela é fixada deitada. Se o suporte (holder) for de outro tipo, ajuste esse valor.
