// Pinos do motor esquerdo
const int IN1 = 8;
const int IN2 = 9;

// Pinos do motor direito
const int IN3 = 6;
const int IN4 = 7;

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  Serial.begin(9600);
  Serial.println("Iniciando teste dos motores...");
}

void loop() {
  Serial.println("Andando para frente");
  frente();
  delay(2000);

  Serial.println("Parado");
  parar();
  delay(1000);

  Serial.println("Andando para trás");
  re();
  delay(2000);

  Serial.println("Parado");
  parar();
  delay(1000);
}

// ----- Funções de movimento -----

void frente() {
  // Motor esquerdo gira para frente
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  // Motor direito gira para frente
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void re() {
  // Motor esquerdo gira para trás
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  // Motor direito gira para trás
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void parar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}
