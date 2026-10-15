/*
 * CP2 TinyML - Coleta de dados na ESP32-S3 do módulo de câmera do kit LAFVIN
 *
 * O HC-SR04 fica no shield da placa R3. A R3 (r3/carrinho_r3) amostra o sensor
 * a 20 Hz e, ao receber {"N":111,"D1":1}, envia cada leitura pelo cabo
 * "Camera UART" no formato "<millis>,<distancia_cm>".
 *
 * Este sketch liga esse envio, recebe as leituras na ESP32 (Serial2, 9600) e
 * repassa para o PC pela USB (115200) no formato de scripts/coleta_serial.py.
 *
 * Placa: ESP32S3 Dev Module, Flash 8MB QIO, PSRAM OPI, "8M with spiffs".
 */

const int PINO_RX_R3 = 3;    // cabo Camera UART (igual ao firmware da LAFVIN)
const int PINO_TX_R3 = 40;
const int PINO_LED = 46;
const unsigned long REENVIO_MS = 2000;   // religa o envio se a R3 reiniciar

String linha;
unsigned long ultimaLeitura = 0;

void ligarEnvioR3() { Serial2.print("{\"N\":111,\"D1\":1}"); }

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

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, PINO_RX_R3, PINO_TX_R3);
  pinMode(PINO_LED, OUTPUT);
  delay(500);
  ligarEnvioR3();
}

void loop() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c != '\n') { linha += c; continue; }
    unsigned long t;
    int cm;
    if (lerAmostra(linha, t, cm)) {
      Serial.printf("%lu,%d\n", t, cm);
      ultimaLeitura = millis();
      digitalWrite(PINO_LED, !digitalRead(PINO_LED));
    }
    linha = "";
  }

  static unsigned long ultimoPedido = 0;
  if (millis() - ultimaLeitura > REENVIO_MS && millis() - ultimoPedido > REENVIO_MS) {
    ultimoPedido = millis();
    ligarEnvioR3();
  }
}
