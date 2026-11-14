/*
 * GS CitizenTech - "Fiscal de Calçadas"
 * ESP32-S3 do módulo de câmera do kit LAFVIN (evolução do CP2)
 *
 *   HC-SR04 + seguidor de linha (piso) + bateria ──► R3 (20 Hz)
 *        "millis,cm,L,M,R,V10,desnivel" ──► ESP32-S3
 *                                            ├─ modelo TinyML (obstáculo à frente)
 *                                            ├─ detecção de desnível (buraco/degrau/meio-fio)
 *                                            ├─ decisão autônoma (normal / cauteloso / bateria)
 *                                            ├─ registro de ocorrências
 *                                            └─ Wi-Fi: /status /eventos /cmd ──► painel com IA (gs/painel)
 *        {"N":110,"D1":dir,"D2":vel,"D3":classe} ◄──┘
 *
 * A R3 (gs/r3/carrinho_r3) só executa as ações no modo TinyML (tecla 0 do controle IR,
 * ou botão "Iniciar patrulha" no painel). Ela para sozinha se a ESP32 ficar 1 s calada
 * e recusa andar para frente quando o sensor de piso detecta um desnível.
 *
 *   livre       -> frente rápido   (LED verde)
 *   aproximando -> frente devagar  (LED laranja)
 *   obstaculo   -> ré e gira       (LED vermelho)   + ocorrência "obstaculo"
 *   desnível    -> para, ré e gira (LED roxo)       + ocorrência "desnivel"
 *
 * USB (115200):
 *   "millis,cm"   leituras (compatível com os scripts do CP2)
 *   "# ..."       inferências e mensagens
 *   "@T {...}"    telemetria em JSON (2x por segundo) para o painel
 *   "@E {...}"    cada ocorrência em JSON, assim que acontece
 *
 * Wi-Fi: copie config_wifi.example.h para config_wifi.h e preencha a rede.
 * Sem config_wifi.h (ou sem rede) o robô funciona igual, só sem o painel por Wi-Fi.
 *
 * Placa: ESP32S3 Dev Module, Flash 8MB QIO, PSRAM OPI, "8M with spiffs". Core esp32 2.0.14.
 */

#include <cp2_tinyml_inferencing.h>   // cp2/edge_impulse/biblioteca/ei-cp2_tinyml-arduino-1.0.1.zip
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#if __has_include("config_wifi.h")
#include "config_wifi.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_SENHA ""
#endif

const int PINO_RX_R3 = 3;    // cabo Camera UART (igual ao firmware da LAFVIN)
const int PINO_TX_R3 = 40;
const int PINO_LED = 46;

// ---------- Janela do modelo (igual à do treino: 20 Hz, 1 s = 20 leituras) ----------
const int N_JANELA = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
const unsigned long INTERVALO_INFERENCIA_MS = 250;
const unsigned long REENVIO_MS = 2000;
const float CONFIANCA_MIN = 0.60;

// ---------- Ações (comando 110 da R3) ----------
const int DIR_FRENTE = 1, DIR_RE = 2, DIR_DIREITA = 4, DIR_PARAR = 9;
const int VEL_RAPIDO = 150, VEL_DEVAGAR = 90;               // modo normal
const int VEL_RAPIDO_CAUTELA = 110, VEL_DEVAGAR_CAUTELA = 75; // trecho crítico ou bateria baixa
const int VEL_MANOBRA = 150;
const unsigned long TEMPO_RE_MS = 500;
const unsigned long TEMPO_GIRO_MS = 600;
const unsigned long TEMPO_PARADA_DESNIVEL_MS = 300;
const unsigned long TEMPO_RE_DESNIVEL_MS = 700;
const unsigned long REPETE_ACAO_MS = 100;    // mantém a R3 viva (timeout dela é 1 s)
const int DISTANCIA_SEGURANCA_CM = 20;

// ---------- Sensor de piso (seguidor de linha) ----------
int limiarPiso = 850;                         // enviado à R3 (comando 112); ajustável pelo painel

// ---------- Autonomia: trecho crítico e bateria ----------
const unsigned long JANELA_CRITICA_MS = 30000;  // 2+ ocorrências em 30 s = trecho crítico
const int OCORRENCIAS_CRITICAS = 2;
const unsigned long DURACAO_CAUTELA_MS = 30000;
const int BATERIA_BAIXA_V10 = 70;               // 7,0 V (2 células de lítio)
const int BATERIA_OK_V10 = 72;
const unsigned long BATERIA_CONFIRMA_MS = 5000;

enum Classe { LIVRE, APROXIMANDO, OBSTACULO, INCERTO, DESNIVEL };
const char* NOME_CLASSE[] = {"livre", "aproximando", "obstaculo", "incerto", "desnivel"};

enum TipoEvento { EV_OBSTACULO, EV_DESNIVEL, EV_TRECHO_CRITICO, EV_BATERIA_BAIXA,
                  EV_FALHA_R3, EV_R3_OK, EV_PISO_INDISPONIVEL, EV_PATRULHA };
const char* NOME_EVENTO[] = {"obstaculo", "desnivel", "trecho_critico", "bateria_baixa",
                             "falha_r3", "r3_reconectada", "sensor_piso_indisponivel", "patrulha"};

// ---------- Estado dos sensores ----------
float janela[N_JANELA];
int posJanela = 0;
int amostras = 0;
unsigned long ultimaLeitura = 0;
String linha;

int distAtual = 150;
int pisoL = 0, pisoM = 0, pisoR = 0;
int bateriaV10 = 0;
bool desnivel = false;
bool pisoDisponivel = false;       // R3 com firmware da GS enviando o sensor de piso
float prob[3] = {0, 0, 0};         // aproximando, livre, obstaculo (ordem do modelo)
String nomeProb[3] = {"", "", ""};
Classe ultimaPredicao = INCERTO;
int tempoInferenciaMs = 0;

// ---------- Estado da decisão ----------
int acaoDir = DIR_PARAR, acaoVel = 0;
Classe classeAtual = INCERTO;
enum Manobra { NENHUMA, RE, GIRO, PARA_DESNIVEL, RE_DESNIVEL, GIRO_DESNIVEL } manobra = NENHUMA;
const char* NOME_MANOBRA[] = {"nenhuma", "re", "giro", "parada_desnivel", "re_desnivel", "giro_desnivel"};
bool pausado = false;
unsigned long inicioManobra = 0;
bool cautela = false;
unsigned long fimCautela = 0;
bool bateriaBaixa = false;
unsigned long inicioBateriaBaixa = 0;
bool r3Ok = false;

// ---------- Ocorrências ----------
struct Evento {
  uint32_t id;
  uint32_t t;
  uint8_t tipo;
  int dist, l, m, r, v10;
  float confianca;
};
const int N_EVENTOS = 40;
Evento eventos[N_EVENTOS];
uint32_t proximoIdEvento = 1;
uint32_t contagem[8] = {0};
unsigned long recentes[8];          // instantes das últimas ocorrências de obstáculo/desnível
int posRecentes = 0;

WebServer servidor(80);
bool wifiAnunciado = false;

// ================= Comunicação com a R3 =================

void ligarEnvioR3() {
  Serial2.print("{\"N\":111,\"D1\":2}");   // GS: distância + piso + bateria
  delay(20);
  Serial2.printf("{\"N\":112,\"D1\":%d}", limiarPiso);
}

int codigoClasse(Classe c) {
  if (c == DESNIVEL) return 3;
  if (c == INCERTO) return 255;
  return (int)c;
}

void enviarAcao(int dir, int vel, Classe c) {
  acaoDir = dir;
  acaoVel = vel;
  Serial2.printf("{\"N\":110,\"D1\":%d,\"D2\":%d,\"D3\":%d}", dir, vel, codigoClasse(c));
}

// aceita "millis,cm" (R3 do CP2) e "millis,cm,L,M,R,V10,desnivel" (R3 da GS)
// "{ok}1163,30" -> descarta o que vier antes do último '}'
bool lerAmostra(String s) {
  s = s.substring(s.lastIndexOf('}') + 1);
  s.trim();
  if (s.length() == 0 || !isDigit(s[0])) return false;
  unsigned long t;
  int cm, l, m, r, v, d;
  int n = sscanf(s.c_str(), "%lu,%d,%d,%d,%d,%d,%d", &t, &cm, &l, &m, &r, &v, &d);
  if (n != 2 && n != 7) return false;
  if (cm < 0 || cm > 400) return false;

  distAtual = cm;
  janela[posJanela] = cm;
  posJanela = (posJanela + 1) % N_JANELA;
  if (amostras < N_JANELA) amostras++;

  if (n == 7) {
    pisoL = l; pisoM = m; pisoR = r;
    bateriaV10 = v;
    desnivel = d == 1;
    pisoDisponivel = true;
  } else {
    pisoDisponivel = false;
    desnivel = false;
  }
  Serial.printf("%lu,%d\n", t, cm);
  return true;
}

void receberDaR3() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c != '\n') {
      if (linha.length() < 120) linha += c;
      continue;
    }
    if (lerAmostra(linha)) ultimaLeitura = millis();
    linha = "";
  }
}

// ================= Ocorrências =================

void registrarEvento(TipoEvento tipo, float confianca) {
  Evento& e = eventos[(proximoIdEvento - 1) % N_EVENTOS];
  e.id = proximoIdEvento++;
  e.t = millis();
  e.tipo = tipo;
  e.dist = distAtual;
  e.l = pisoL; e.m = pisoM; e.r = pisoR;
  e.v10 = bateriaV10;
  e.confianca = confianca;
  contagem[tipo]++;
  Serial.printf("@E {\"id\":%lu,\"t\":%lu,\"tipo\":\"%s\",\"dist\":%d,\"piso\":[%d,%d,%d],\"bateria\":%.1f,\"confianca\":%.2f,\"cautela\":%s}\n",
                (unsigned long)e.id, (unsigned long)e.t, NOME_EVENTO[tipo], e.dist, e.l, e.m, e.r,
                e.v10 / 10.0, confianca, cautela ? "true" : "false");

  // autonomia: muitas barreiras em pouco tempo = trecho crítico, anda com cautela
  if (tipo == EV_OBSTACULO || tipo == EV_DESNIVEL) {
    recentes[posRecentes] = millis();
    posRecentes = (posRecentes + 1) % 8;
    int n = 0;
    for (int i = 0; i < 8; i++)
      if (recentes[i] != 0 && millis() - recentes[i] < JANELA_CRITICA_MS) n++;
    if (n >= OCORRENCIAS_CRITICAS) {
      bool novo = !cautela;
      cautela = true;
      fimCautela = millis() + DURACAO_CAUTELA_MS;
      if (novo) registrarEvento(EV_TRECHO_CRITICO, 0);
    }
  }
}

void atualizarAutonomia() {
  if (cautela && (long)(millis() - fimCautela) > 0) {
    cautela = false;
    Serial.println("# fim do modo cauteloso");
  }
  if (!pisoDisponivel || bateriaV10 <= 0) return;
  if (bateriaV10 < BATERIA_BAIXA_V10) {
    if (inicioBateriaBaixa == 0) inicioBateriaBaixa = millis();
    if (!bateriaBaixa && millis() - inicioBateriaBaixa > BATERIA_CONFIRMA_MS) {
      bateriaBaixa = true;
      registrarEvento(EV_BATERIA_BAIXA, 0);
    }
  } else {
    inicioBateriaBaixa = 0;
    if (bateriaV10 >= BATERIA_OK_V10) bateriaBaixa = false;
  }
}

int velRapido() { return (cautela || bateriaBaixa) ? VEL_RAPIDO_CAUTELA : VEL_RAPIDO; }
int velDevagar() { return (cautela || bateriaBaixa) ? VEL_DEVAGAR_CAUTELA : VEL_DEVAGAR; }

// ================= Comandos do PC (USB) =================

void executarComando(const String& acao, int valor);

// comandos JSON digitados no PC (ex.: {"N":101,"D1":4}) são repassados à R3
void repassarDoPC() {
  static String comando;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '{') comando = "";
    comando += c;
    if (c == '}') {
      if (comando == "{\"P\":1}") executarComando("pausar", 0);
      else if (comando == "{\"P\":0}") executarComando("retomar", 0);
      else if (comando == "{\"G\":1}") executarComando("iniciar", 0);
      else if (comando == "{\"G\":0}") executarComando("parar", 0);
      else {
        Serial2.print(comando);
        Serial.printf("# repassado a R3: %s\n", comando.c_str());
      }
      comando = "";
    }
    if (comando.length() > 100) comando = "";
  }
}

void executarComando(const String& acao, int valor) {
  if (acao == "iniciar") {          // entra no modo TinyML na R3 (o mesmo que a tecla 0)
    pausado = false;
    Serial2.print("{\"N\":101,\"D1\":4}");
    registrarEvento(EV_PATRULHA, 1);
  } else if (acao == "parar") {     // standby na R3
    enviarAcao(DIR_PARAR, 0, classeAtual);
    Serial2.print("{\"N\":101,\"D1\":0}");
    registrarEvento(EV_PATRULHA, 0);
  } else if (acao == "pausar") {
    pausado = true;
    enviarAcao(DIR_PARAR, 0, classeAtual);
  } else if (acao == "retomar") {
    pausado = false;
  } else if (acao == "limiar" && valor >= 100 && valor <= 1023) {
    limiarPiso = valor;
    Serial2.printf("{\"N\":112,\"D1\":%d}", limiarPiso);
  }
  Serial.printf("# comando: %s %d\n", acao.c_str(), valor);
}

// menor das últimas 3 leituras: uma leitura sem eco (150) não esconde a parede
int distanciaRecente() {
  int menor = 150;
  for (int i = 1; i <= 3 && i <= amostras; i++)
    menor = min(menor, (int)janela[(posJanela - i + N_JANELA) % N_JANELA]);
  return menor;
}

bool pertoDemais() { return distanciaRecente() < DISTANCIA_SEGURANCA_CM; }

// ================= Inferência (TinyML do CP2) =================

void janelaOrdenada(float* destino) {
  for (int i = 0; i < N_JANELA; i++) destino[i] = janela[(posJanela + i) % N_JANELA];
}

float confiancaUltima = 0;

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
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT && i < 3; i++) {
    const char* rotulo = resultado.classification[i].label;
    float p = resultado.classification[i].value;
    prob[i] = p;
    nomeProb[i] = rotulo;
    Serial.printf("%s=%.2f ", rotulo, p);
    if (p > maior) {
      maior = p;
      melhor = INCERTO;
      for (int c = LIVRE; c <= OBSTACULO; c++)
        if (strcmp(rotulo, NOME_CLASSE[c]) == 0) melhor = (Classe)c;
    }
  }
  tempoInferenciaMs = resultado.timing.dsp + resultado.timing.classification;
  Serial.printf("(%d ms)", tempoInferenciaMs);
  confiancaUltima = maior;
  ultimaPredicao = maior >= CONFIANCA_MIN ? melhor : INCERTO;
  return ultimaPredicao;
}

void agir(Classe c) {
  bool perto = pertoDemais();
  Serial.printf(" -> %s%s%s\n", NOME_CLASSE[c], perto && c != OBSTACULO ? " (parado: muito perto)" : "",
                cautela ? " [cautela]" : "");
  if (c == INCERTO) return;  // mantém a ação anterior
  classeAtual = c;
  if (perto && c != OBSTACULO) {
    enviarAcao(DIR_PARAR, 0, c);
    return;
  }
  switch (c) {
    case LIVRE:       enviarAcao(DIR_FRENTE, velRapido(), c);  break;
    case APROXIMANDO: enviarAcao(DIR_FRENTE, velDevagar(), c); break;
    case OBSTACULO:
      registrarEvento(EV_OBSTACULO, confiancaUltima);
      manobra = RE;
      inicioManobra = millis();
      enviarAcao(DIR_RE, VEL_MANOBRA, c);
      break;
    default: break;
  }
}

// desnível tem prioridade sobre tudo: para, dá ré e gira para sair da borda
// novo = borda recém-detectada (gera ocorrência); senão é só uma nova tentativa de sair dela
unsigned long ultimoDesvioDesnivel = 0;
void iniciarDesvioDesnivel(bool novo) {
  if (novo) registrarEvento(EV_DESNIVEL, 1.0);
  ultimoDesvioDesnivel = millis();
  classeAtual = DESNIVEL;
  manobra = PARA_DESNIVEL;
  inicioManobra = millis();
  enviarAcao(DIR_PARAR, 0, DESNIVEL);
  Serial.println("# DESNIVEL detectado pelo sensor de piso -> para, ré e gira");
}

// manobras sem travar o loop
bool executandoManobra() {
  if (manobra == NENHUMA) return false;
  unsigned long t = millis() - inicioManobra;
  switch (manobra) {
    case RE:
      if (t > TEMPO_RE_MS) { manobra = GIRO; inicioManobra = millis(); enviarAcao(DIR_DIREITA, VEL_MANOBRA, OBSTACULO); }
      break;
    case GIRO:
      if (t > TEMPO_GIRO_MS) { manobra = NENHUMA; enviarAcao(DIR_PARAR, 0, OBSTACULO); amostras = 0; }
      break;
    case PARA_DESNIVEL:
      if (t > TEMPO_PARADA_DESNIVEL_MS) { manobra = RE_DESNIVEL; inicioManobra = millis(); enviarAcao(DIR_RE, VEL_MANOBRA, DESNIVEL); }
      break;
    case RE_DESNIVEL:
      if (t > TEMPO_RE_DESNIVEL_MS) { manobra = GIRO_DESNIVEL; inicioManobra = millis(); enviarAcao(DIR_DIREITA, VEL_MANOBRA, DESNIVEL); }
      break;
    case GIRO_DESNIVEL:
      if (t > TEMPO_GIRO_MS) { manobra = NENHUMA; enviarAcao(DIR_PARAR, 0, DESNIVEL); amostras = 0; }
      break;
    default: manobra = NENHUMA; break;
  }
  return manobra != NENHUMA;
}

// ================= Wi-Fi: API para o painel =================

String jsonStatus() {
  char buf[900];
  snprintf(buf, sizeof(buf),
    "{\"uptime_ms\":%lu,\"r3_ok\":%s,\"dist_cm\":%d,"
    "\"piso\":{\"disponivel\":%s,\"l\":%d,\"m\":%d,\"r\":%d,\"limiar\":%d,\"desnivel\":%s},"
    "\"bateria_v\":%.1f,\"bateria_baixa\":%s,"
    "\"modelo\":{\"classe\":\"%s\",\"confianca\":%.2f,\"tempo_ms\":%d,\"probs\":{\"%s\":%.2f,\"%s\":%.2f,\"%s\":%.2f}},"
    "\"decisao\":{\"classe\":\"%s\",\"dir\":%d,\"vel\":%d,\"manobra\":\"%s\",\"cautela\":%s,\"pausado\":%s},"
    "\"contagem\":{\"obstaculo\":%lu,\"desnivel\":%lu,\"trecho_critico\":%lu,\"bateria_baixa\":%lu,\"falha_r3\":%lu},"
    "\"ultimo_evento\":%lu,\"wifi_rssi\":%d}",
    millis(), r3Ok ? "true" : "false", distAtual,
    pisoDisponivel ? "true" : "false", pisoL, pisoM, pisoR, limiarPiso, desnivel ? "true" : "false",
    bateriaV10 / 10.0, bateriaBaixa ? "true" : "false",
    NOME_CLASSE[ultimaPredicao], confiancaUltima, tempoInferenciaMs,
    nomeProb[0].c_str(), prob[0], nomeProb[1].c_str(), prob[1], nomeProb[2].c_str(), prob[2],
    NOME_CLASSE[classeAtual], acaoDir, acaoVel, NOME_MANOBRA[manobra],
    cautela ? "true" : "false", pausado ? "true" : "false",
    (unsigned long)contagem[EV_OBSTACULO], (unsigned long)contagem[EV_DESNIVEL],
    (unsigned long)contagem[EV_TRECHO_CRITICO], (unsigned long)contagem[EV_BATERIA_BAIXA],
    (unsigned long)contagem[EV_FALHA_R3],
    (unsigned long)(proximoIdEvento - 1), WiFi.RSSI());
  return String(buf);
}

String jsonEventos(uint32_t desde) {
  String s = "[";
  bool primeiro = true;
  uint32_t inicio = proximoIdEvento > N_EVENTOS ? proximoIdEvento - N_EVENTOS : 1;
  for (uint32_t id = max(inicio, desde + 1); id < proximoIdEvento; id++) {
    const Evento& e = eventos[(id - 1) % N_EVENTOS];
    char buf[220];
    snprintf(buf, sizeof(buf),
      "%s{\"id\":%lu,\"t\":%lu,\"tipo\":\"%s\",\"dist\":%d,\"piso\":[%d,%d,%d],\"bateria\":%.1f,\"confianca\":%.2f}",
      primeiro ? "" : ",", (unsigned long)e.id, (unsigned long)e.t, NOME_EVENTO[e.tipo],
      e.dist, e.l, e.m, e.r, e.v10 / 10.0, e.confianca);
    s += buf;
    primeiro = false;
  }
  return s + "]";
}

void responderJson(const String& corpo) {
  servidor.sendHeader("Access-Control-Allow-Origin", "*");
  servidor.send(200, "application/json", corpo);
}

void configurarServidor() {
  servidor.on("/", []() {
    servidor.send(200, "text/html; charset=utf-8",
      "<h3>Fiscal de Cal&ccedil;adas - ESP32</h3><p>API: <a href='/status'>/status</a> "
      "&middot; <a href='/eventos'>/eventos</a> &middot; /cmd?acao=iniciar|parar|pausar|retomar|limiar&amp;valor=N</p>"
      "<p>Abra o painel com IA no PC (gs/painel).</p>");
  });
  servidor.on("/status", []() { responderJson(jsonStatus()); });
  servidor.on("/eventos", []() {
    uint32_t desde = servidor.hasArg("desde") ? servidor.arg("desde").toInt() : 0;
    responderJson(jsonEventos(desde));
  });
  servidor.on("/cmd", []() {
    String acao = servidor.arg("acao");
    int valor = servidor.hasArg("valor") ? servidor.arg("valor").toInt() : 0;
    executarComando(acao, valor);
    responderJson("{\"ok\":true,\"acao\":\"" + acao + "\"}");
  });
  servidor.begin();
}

void cuidarWifi() {
  if (strlen(WIFI_SSID) == 0) return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiAnunciado) {
      wifiAnunciado = true;
      MDNS.begin("fiscal");
      Serial.printf("# wifi conectado: http://%s  (ou http://fiscal.local)\n", WiFi.localIP().toString().c_str());
    }
    servidor.handleClient();
  } else if (wifiAnunciado) {
    wifiAnunciado = false;
    Serial.println("# wifi caiu - o robô continua autônomo; reconectando...");
  }
}

// ================= Setup / loop =================

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, PINO_RX_R3, PINO_TX_R3);
  pinMode(PINO_LED, OUTPUT);
  delay(500);
  Serial.printf("# GS Fiscal de Calcadas - modelo %s, janela de %d leituras, %d classes\n",
                EI_CLASSIFIER_PROJECT_NAME, N_JANELA, EI_CLASSIFIER_LABEL_COUNT);
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
    configurarServidor();
    Serial.printf("# conectando ao wifi \"%s\" (sem bloquear o robô)\n", WIFI_SSID);
  } else {
    Serial.println("# sem config_wifi.h: modo offline (painel só pela USB)");
  }
  ligarEnvioR3();
}

void loop() {
  receberDaR3();
  repassarDoPC();
  cuidarWifi();
  atualizarAutonomia();

  // telemetria para o painel pela USB
  static unsigned long ultimaTelemetria = 0;
  if (millis() - ultimaTelemetria > 500) {
    ultimaTelemetria = millis();
    Serial.print("@T ");
    Serial.println(jsonStatus());
  }

  // R3 reiniciou ou cabo solto: registra a falha, pede o envio de novo e não age às cegas
  static unsigned long ultimoPedido = 0;
  if (millis() - ultimaLeitura > REENVIO_MS) {
    digitalWrite(PINO_LED, LOW);
    if (r3Ok) { r3Ok = false; registrarEvento(EV_FALHA_R3, 0); }
    if (millis() - ultimoPedido > REENVIO_MS) {
      ultimoPedido = millis();
      amostras = 0;
      ligarEnvioR3();
    }
    return;
  }
  if (!r3Ok) {
    r3Ok = true;
    if (contagem[EV_FALHA_R3] > 0) registrarEvento(EV_R3_OK, 0);
  }

  // R3 mandando só "millis,cm": pede o formato da GS de novo a cada 3 s; se depois de
  // 6 s continuar sem o sensor de piso (R3 com firmware antigo), avisa e segue só com o ultrassônico
  static bool avisouPiso = false;
  static unsigned long semPisoDesde = 0, ultimoPedidoPiso = 0;
  if (!pisoDisponivel) {
    if (semPisoDesde == 0) semPisoDesde = millis();
    if (millis() - ultimoPedidoPiso > 3000) { ultimoPedidoPiso = millis(); ligarEnvioR3(); }
    if (!avisouPiso && millis() - semPisoDesde > 6000) {
      avisouPiso = true;
      registrarEvento(EV_PISO_INDISPONIVEL, 0);
    }
  } else {
    semPisoDesde = 0;
    avisouPiso = false;
  }

  digitalWrite(PINO_LED, HIGH);
  if (pausado) return;

  // desnível interrompe qualquer coisa, menos a própria manobra de desnível
  // (robô levantado do chão também conta como desnível: ele se recusa a andar)
  static bool desnivelAnterior = false;
  bool emManobraDesnivel = manobra == PARA_DESNIVEL || manobra == RE_DESNIVEL || manobra == GIRO_DESNIVEL;
  if (desnivel && !emManobraDesnivel && manobra != RE) {
    bool novo = !desnivelAnterior;
    if (novo || millis() - ultimoDesvioDesnivel > 3000) iniciarDesvioDesnivel(novo);
  }
  desnivelAnterior = desnivel;

  if (!executandoManobra()) {
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
