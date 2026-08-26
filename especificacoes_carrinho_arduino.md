# Especificação de Projeto: Carrinho Robô Baseado em Arduino (MVP & Gestão Maker)

Este documento especifica o escopo, priorização de funcionalidades, backlog, dependências, fluxo de trabalho Kanban e planilha de custos para a construção do **Carrinho-Robô baseado em Arduino**, em conformidade com as diretrizes do **Slide 16 da Aula 17 (Project-based Maker Lab - FIAP)**.

---

## 1. Definição do MVP vs. Produto Concluído

| Etapa | Critérios de Aceitação / Funcionalidades |
| :--- | :--- |
| **MVP (Mínimo Viável)** | - Chassi (4 rodas);<br>- Microcontrolador Arduino (Uno/Nano/Mega);<br>- Ponte H (L298N ou similar) para acionamento dos motores DC;<br>- Fonte de alimentação (suporte de pilhas/bateria com chave L/D);<br>- Controle básico de movimentação (Avançar, Recuar, Virar à Esquerda, Virar à Direita, Parar via Bluetooth/RF/Serial). |
| **Produto Concluído** | - Carenagem de proteção cortada a laser (MDF/Acrílico) ou impressa em 3D;<br>- Desvio de obstáculos usando Sensor Ultrassônico;<br>- Faróis/Lanternas indicadoras com LEDs;|

---

## 2. Matriz MoSCoW

| Categoria | Descrição | Funcionalidades / Componentes do Projeto |
| :--- | :--- | :--- |
| **Must Have** *(Obrigatório)* | Essencial para o carrinho andar e ser operado no MVP | - Microcontrolador Arduino + Cabo USB;<br>- Motor de Passo / Motores DC com caixa de redução (2x ou 4x);<br>- Módulo Driver Ponte H (L298N);<br>- Chassi base de acrílico/MDF com rodas e roda boba;<br>- Pack de baterias / suporte de pilhas de alimentação dos motores;<br>- Firmware básico de controle de ponte H (PWM e direção). |
| **Should Have** *(Importante)* | Recursos de alta relevância para usabilidade e autonomia | - Módulo Bluetooth (HC-05 / HC-06) para controle sem fio via aplicativo mobile;<br>- Sensor de distância Ultrassônico (HC-SR04) para desvio básico de obstáculos;<br>- Chave gangorra L/D (Power Switch) integrada no chassi. |
| **Could Have** *(Desejável)* | Melhorias estéticas e de telemetria | - Carenagem personalizada (MDF cortado a laser ou Impressão 3D);<br>- Faróis frontais e luzes de ré com LEDs RGB;<br>- Buzzer de ré ou sinalizador sonoro;<br>- Servo motor para varredura angular do sensor ultrassônico. |
| **Won't Have (por enquanto)** | Recursos descartados no escopo atual para evitar *Feature Creep* | - Leitor de RFID;<br>- Módulo GPS / Câmera de transmissão de vídeo (FPV);<br>- Display OLED de alta resolução;<br>- Integração em Nuvem / Wi-Fi IoT avançado. |

---

## 3. Backlog do Projeto & Atribuição de Tarefas

| ID | Categoria | Descrição da Tarefa | Responsável | Status Inicial |
| :--- | :--- | :--- | :--- | :--- |
| **TSK-01** | Hardware | Medição de componentes e projeto 2D/3D do chassi | Desenvolvedor Hardware / Maker | A Fazer |
| **TSK-02** | Hardware | Corte de chassi (laser/impressão) e montagem mecânica | Maker / Montador | A Fazer |
| **TSK-03** | Eletrônica | Montagem do circuito: Arduino, Ponte H L298N, motores e alimentação | Engenheiro Eletrônico | A Fazer |
| **TSK-04** | Software | Firmware Arduino para controle de ponte H (movimentação básica) | Desenvolvedor Software | A Fazer |
| **TSK-05** | Software | Integração do Módulo Bluetooth (HC-05) e protocolo de comandos | Desenvolvedor Software | A Fazer |
| **TSK-06** | Testes | Validação do MVP (teste de tração, resposta de comandos e estabilidade) | Equipe QA / Todos | A Fazer |
| **TSK-07** | Expansão | Instalação e programação do Sensor Ultrassônico + Servo | Desenvolvedor Software/Hardware | A Fazer |
| **TSK-08** | Acabamento | Design, montagem da carenagem (MDF/Papelão/3D) e organização dos cabos | Maker / Designer | A Fazer |

---

## 4. Árvore de Dependências

Para evitar gargalos, as etapas devem seguir a sequência de dependências abaixo:

```text
1. Medir Componentes (Arduino, Motores, Bateria, Ponte H)
   └── 2. Desenho 2D/3D do Chassi
        └── 3. Fabricação / Impressão do Chassi
             └── 4. Montagem Mecânica (Motores + Rodas + Roda Boba)
                  └── 5. Montagem Eletrônica (Fiação, Ponte H, Alimentação e Arduino)
                       └── 6. Testes dos Motores (Direção e PWM)
                            └── 7. Desenvolvimento do Software Básico
                                 └── 8. VALIDAÇÃO DO MVP (Carrinho anda e responde a comandos)
                                      ├── 9. Adição de Sensores (Ultrassônico / Bluetooth)
                                      └── 10. Construção da Carenagem Final (MDF / 3D / Papelão)
```

---

## 5. Estrutura do Quadro Kanban

O acompanhamento de tarefas deve utilizar as seguintes colunas no GitHub Projects:

```text
+-------------------+-------------------+-------------------+-------------------+
|    BACKLOG        |    TO DO          |   IN PROGRESS     |      DONE         |
|   (A Fazer)       |  (Para Fazer)     |  (Em Andamento)   |   (Concluído)     |
+-------------------+-------------------+-------------------+-------------------+
| - TSK-07 (Ultrass)| - TSK-01 (Medição)|                   |                   |
| - TSK-08 (Carenag)| - TSK-02 (Chassi) |                   |                   |
|                   | - TSK-03 (Circuito|                   |                   |
|                   | - TSK-04 (Soft Básico)                |                   |
|                   | - TSK-05 (Bluetooth)                  |                   |
|                   | - TSK-06 (Valida MVP)                 |                   |
+-------------------+-------------------+-------------------+-------------------+
```

---

## 6. Planilha de Custos Estimados (Kit Unificado)

Para este projeto, foi adquirido o kit unificado **LAFVIN R3 Smart Robot Car Kit com Câmera** [cite: 1.1.2], contendo todas as peças de hardware, sensores e eletrônica necessárias em um único pacote [cite: 1.1.2].

### Detalhamento das Peças Incluídas no Kit
- Placa principal LAFVIN R3 (Compatível com ecossistema Arduino) [cite: 1.1.2]
- Módulo Wi-Fi ESP32-S3 para controle remoto [cite: 1.1.2]
- Câmera OV3660 para transmissão de vídeo FPV em tempo real [cite: 1.1.2]
- Sensor de distância Ultrassônico (para desvio de obstáculos) [cite: 1.1.2]
- Módulo de sensores de seguimento de linha (Line Tracking) [cite: 1.1.2]
- Kit Driver de Motor / Ponte H [cite: 1.1.2]
- Motores DC com caixa de redução e rodas [cite: 1.1.2]
- Chassi estrutural com suportes de fixação [cite: 1.1.2]
- Suporte para baterias/pilhas e cabo de conexão [cite: 1.1.2]
- Kit de cabos de conexão à prova de erros [cite: 1.1.2]

### Tabela de Custo do Projeto

| Item / Produto | Preço Único do Kit (R$) | Link de Referência |
| :--- | :---: | :--- |
| **LAFVIN R3 Smart Robot Car Kit com Câmera para Arduino** (Kit Completo de Robótica STEM) [cite: 1.1.2] | **R$ 389,00** | [Amazon Brasil](https://www.amazon.com.br/dp/B0GYSKG52P) |
| **TOTAL DO PROJETO** | **R$ 389,00** | -- |