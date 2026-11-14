/*
 * CP2 TinyML - ESP32-S3 do módulo de câmera do kit LAFVIN
 *
 *   HC-SR04 ──► R3 (amostra a 20 Hz) ──"millis,cm"──► ESP32-S3 (janela de 1 s + modelo TinyML)
 *                ▲                                          │
 *                └────────── {"N":110,"D1":dir,"D2":vel,"D3":classe} ◄──┘
 *
 * A ESP32 recebe as distâncias pelo cabo "Camera UART" (Serial2, 9600), roda o
 * modelo do Edge Impulse e devolve a ação. A R3 (r3/carrinho_r3) só executa a ação
 * no modo TinyML (tecla 0 do controle IR), e para sozinha se a ESP32 ficar 1 s calada.
 *
 *   livre       -> frente rápido   (LED da R3 verde)
 *   aproximando -> frente devagar  (LED laranja)
 *   obstaculo   -> ré e gira       (LED vermelho)
 *
 * A USB (115200) mostra as leituras "millis,cm" (para scripts/coleta_serial.py)
 * e as inferências em linhas que começam com '#'.
 *
 * Placa: ESP32S3 Dev Module, Flash 8MB QIO, PSRAM OPI, "8M with spiffs". Core esp32 2.0.14.
 */

// Biblioteca exportada do Edge Impulse (projeto "cp2_tinyml"):
// edge_impulse/biblioteca/ei-cp2_tinyml-arduino-1.0.1.zip
#include <cp2_tinyml_inferencing.h>

const int PINO_RX_R3 = 3;    // cabo Camera UART (igual ao firmware da LAFVIN)
const int PINO_TX_R3 = 40;
const int PINO_LED = 46;

// ---------- Janela (igual à do treino: 20 Hz, 1 s = 20 leituras) ----------
const int N_JANELA = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
const unsigned long INTERVALO_INFERENCIA_MS = 250;
const unsigned long REENVIO_MS = 2000;
const float CONFIANCA_MIN = 0.60;

// ---------- Ações (comando 110 da R3) ----------
const int DIR_FRENTE = 1, DIR_RE = 2, DIR_DIREITA = 4, DIR_PARAR = 9;
const int VEL_RAPIDO = 150;
const int VEL_DEVAGAR = 90;
const unsigned long TEMPO_RE_MS = 500;
const unsigned long TEMPO_GIRO_MS = 600;
const unsigned long REPETE_ACAO_MS = 100;    // mantém a R3 viva (timeout dela é 1 s)
const int DISTANCIA_SEGURANCA_CM = 20;       // segurança: não avança mais perto que isso

enum Classe { LIVRE, APROXIMANDO, OBSTACULO, INCERTO };
const char* NOME_CLASSE[] = {"livre", "aproximando", "obstaculo", "incerto"};

float janela[N_JANELA];
int posJanela = 0;
int amostras = 0;
unsigned long ultimaLeitura = 0;
String linha;

int acaoDir = DIR_PARAR, acaoVel = 0;
Classe classeAtual = INCERTO;
enum Manobra { NENHUMA, RE, GIRO } manobra = NENHUMA;
bool pausado = false;
unsigned long inicioManobra = 0;

// ================= Comunicação com a R3 =================

void ligarEnvioR3() { Serial2.print("{\"N\":111,\"D1\":1}"); }

void enviarAcao(int dir, int vel, Classe c) {
  acaoDir = dir;
  acaoVel = vel;
  Serial2.printf("{\"N\":110,\"D1\":%d,\"D2\":%d,\"D3\":%d}", dir, vel, c == INCERTO ? 255 : (int)c);
}

// "{ok}1163,30" -> descarta o que vier antes do último '}'
bool lerAmostra(String s, unsigned long& t, int& cm) {
  s = s.substring(s.lastIndexOf('}') + 1);
  s.trim();
  int virgula = s.indexOf(',');
  if (virgula <= 0 || s.indexOf(',', virgula + 1) != -1) return false;
  t = s.substring(0, virgula).toInt();
  cm = s.substring(virgula + 1).toInt();
  return isDigit(s[0]) && isDigit(s[virgula + 1]);
}

void receberDaR3() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c != '\n') { linha += c; continue; }
    unsigned long t;
    int cm;
    if (lerAmostra(linha, t, cm)) {
      janela[posJanela] = cm;
      posJanela = (posJanela + 1) % N_JANELA;
      if (amostras < N_JANELA) amostras++;
      ultimaLeitura = millis();
      Serial.printf("%lu,%d\n", t, cm);
    }
    linha = "";
  }
}

// comandos JSON digitados no PC (ex.: {"N":101,"D1":4}) são repassados à R3,
// porque a USB da R3 não consegue falar com ela enquanto o módulo ocupa o RX
void repassarDoPC() {
  static String comando;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '{') comando = "";
    comando += c;
    if (c == '}') {
      if (comando == "{\"P\":1}" || comando == "{\"P\":0}") {
        // pausa o modelo (testes pelo PC): a ESP32 para de mandar ações à R3
        pausado = comando == "{\"P\":1}";
        Serial.printf("# modelo %s\n", pausado ? "PAUSADO" : "ativo");
      } else {
        Serial2.print(comando);
        Serial.printf("# repassado a R3: %s\n", comando.c_str());
      }
      comando = "";
    }
  }
}

// menor das últimas 3 leituras: uma leitura sem eco (150) não esconde a parede
int distanciaRecente() {
  int menor = 150;
  for (int i = 1; i <= 3 && i <= amostras; i++)
    menor = min(menor, (int)janela[(posJanela - i + N_JANELA) % N_JANELA]);
  return menor;
}

bool pertoDemais() { return distanciaRecente() < DISTANCIA_SEGURANCA_CM; }

// ================= Inferência =================

void janelaOrdenada(float* destino) {
  for (int i = 0; i < N_JANELA; i++) destino[i] = janela[(posJanela + i) % N_JANELA];
}

// roda o modelo do Edge Impulse sobre a janela; abaixo de CONFIANCA_MIN = INCERTO
Classe classificar() {
  static float features[N_JANELA];
  janelaOrdenada(features);

  signal_t sinal;
  numpy::signal_from_buffer(features, N_JANELA, &sinal);
  ei_impulse_result_t resultado = {0};
  if (run_classifier(&sinal, &resultado, false) != EI_IMPULSE_OK) return INCERTO;

  Classe melhor = INCERTO;
  float maior = 0;
  Serial.print("# ");
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    const char* rotulo = resultado.classification[i].label;
    float p = resultado.classification[i].value;
    Serial.printf("%s=%.2f ", rotulo, p);
    if (p > maior) {
      maior = p;
      melhor = INCERTO;
      for (int c = LIVRE; c <= OBSTACULO; c++)
        if (strcmp(rotulo, NOME_CLASSE[c]) == 0) melhor = (Classe)c;
    }
  }
  Serial.printf("(%d ms)", resultado.timing.dsp + resultado.timing.classification);
  return maior >= CONFIANCA_MIN ? melhor : INCERTO;
}

void agir(Classe c) {
  bool perto = pertoDemais();
  Serial.printf(" -> %s%s\n", NOME_CLASSE[c], perto && c != OBSTACULO ? " (parado: muito perto)" : "");
  if (c == INCERTO) return;  // mantém a ação anterior
  classeAtual = c;
  if (perto && c != OBSTACULO) {
    // não avança contra a parede; o modelo continua rodando e decide a ré
    // quando reconhecer o obstáculo parado à frente
    enviarAcao(DIR_PARAR, 0, c);
    return;
  }
  switch (c) {
    case LIVRE:       enviarAcao(DIR_FRENTE, VEL_RAPIDO, c);  break;
    case APROXIMANDO: enviarAcao(DIR_FRENTE, VEL_DEVAGAR, c); break;
    case OBSTACULO:
      manobra = RE;
      inicioManobra = millis();
      enviarAcao(DIR_RE, VEL_RAPIDO, c);
      break;
    default: break;
  }
}

// manobra de desvio sem travar o loop: ré -> giro -> volta a classificar
bool executandoManobra() {
  if (manobra == NENHUMA) return false;
  unsigned long t = millis() - inicioManobra;
  if (manobra == RE && t > TEMPO_RE_MS) {
    manobra = GIRO;
    inicioManobra = millis();
    enviarAcao(DIR_DIREITA, VEL_RAPIDO, OBSTACULO);
  } else if (manobra == GIRO && t > TEMPO_GIRO_MS) {
    manobra = NENHUMA;
    enviarAcao(DIR_PARAR, 0, OBSTACULO);
    amostras = 0;  // descarta a janela antiga, que ainda via o obstáculo
  }
  return manobra != NENHUMA;
}

// ================= Setup / loop =================

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, PINO_RX_R3, PINO_TX_R3);
  pinMode(PINO_LED, OUTPUT);
  delay(500);
  Serial.printf("# CP2 TinyML - modelo %s, janela de %d leituras, %d classes\n",
                EI_CLASSIFIER_PROJECT_NAME, N_JANELA, EI_CLASSIFIER_LABEL_COUNT);
  if (EI_CLASSIFIER_INTERVAL_MS != 50)
    Serial.printf("# AVISO: modelo treinado a %.0f ms, a R3 amostra a 50 ms\n", (float)EI_CLASSIFIER_INTERVAL_MS);
  ligarEnvioR3();
}

void loop() {
  receberDaR3();
  repassarDoPC();

  // R3 reiniciou ou cabo solto: pede o envio de novo e não age às cegas
  static unsigned long ultimoPedido = 0;
  if (millis() - ultimaLeitura > REENVIO_MS) {
    digitalWrite(PINO_LED, LOW);
    if (millis() - ultimoPedido > REENVIO_MS) {
      ultimoPedido = millis();
      amostras = 0;
      ligarEnvioR3();
    }
    return;
  }
  digitalWrite(PINO_LED, HIGH);
  if (pausado) return;

  if (!executandoManobra()) {
    // segurança entre inferências: para na hora se chegou perto demais andando
    if (acaoDir == DIR_FRENTE && pertoDemais()) enviarAcao(DIR_PARAR, 0, classeAtual);

    static unsigned long proximaInferencia = 0;
    if (amostras == N_JANELA && millis() >= proximaInferencia) {
      proximaInferencia = millis() + INTERVALO_INFERENCIA_MS;
      agir(classificar());
    }
  }

  static unsigned long ultimoReenvio = 0;
  if (millis() - ultimoReenvio > REPETE_ACAO_MS) {
    ultimoReenvio = millis();
    enviarAcao(acaoDir, acaoVel, classeAtual);
  }
}
