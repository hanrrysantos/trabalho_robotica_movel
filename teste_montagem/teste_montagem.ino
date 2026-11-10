/*
  TESTE DE MONTAGEM - Robô desviador de obstáculos
  Placa: ESP32 DevKit (Arduino-ESP32 core 3.x)
  Nenhuma biblioteca extra: o servo é controlado direto pelo PWM (LEDC) do ESP32

  Direita/esquerda = do ponto de vista do ROBÔ (olhando para onde o sensor aponta).

  Como usar:
  1. Levante o robô (rodas no ar) antes de testar os motores.
  2. Desconecte o power bank do ESP32 para gravar; depois religue para testar
     (o Monitor Serial precisa do USB do PC: nesse caso alimente o ESP32 pelo PC
     e deixe só o cabo cortado do power bank na protoboard).
  3. Abra o Monitor Serial a 115200 baud.
  4. Digite o número do teste e pressione Enter.
  5. Compare o que acontece com o "ESPERADO" impresso na tela.
*/
// =================== PINOS (montagem conferida) ===================
// Sensor HC-SR04
const int PIN_TRIG  = 5;
const int PIN_ECHO  = 33;   // via divisor 1k / 2k
// Servo SG90
const int PIN_SERVO = 13;
// Servos dos braços
const int PIN_BRACO_ESQ = 17;   // TX2
const int PIN_BRACO_DIR = 16;   // RX2
// Driver TB6612FNG
const int PIN_STBY = 4;
// Canal A do driver = roda DIREITA
const int PIN_PWM_DIR = 27;  // PWMA
const int PIN_DIR_IN1 = 25;  // AIN1
const int PIN_DIR_IN2 = 26;  // AIN2
// Canal B do driver = roda ESQUERDA
const int PIN_PWM_ESQ = 23;  // PWMB
const int PIN_ESQ_IN1 = 18;  // BIN1
const int PIN_ESQ_IN2 = 19;  // BIN2

const int PWM_FREQ  = 20000;
const int PWM_RES   = 8;
const int PWM_TESTE = 180;

// Servo SG90 via PWM do ESP32: 50 Hz, pulso de 500 us (0°) a 2400 us (180°)
const int SERVO_FREQ = 50;
const int SERVO_RES  = 14;   // 0 a 16383

void escreverServo(int pino, int angulo) {
  angulo = constrain(angulo, 0, 180);
  long pulsoUs = map(angulo, 0, 180, 500, 2400);
  ledcWrite(pino, pulsoUs * 16383L / 20000L);
}

void servoAngulo(int angulo) { escreverServo(PIN_SERVO, angulo); }

void motor(int pwmPin, int in1, int in2, int vel) {
  vel = constrain(vel, -255, 255);
  digitalWrite(in1, vel > 0 ? HIGH : LOW);
  digitalWrite(in2, vel < 0 ? HIGH : LOW);
  ledcWrite(pwmPin, abs(vel));
}

void pararTudo() {
  motor(PIN_PWM_DIR, PIN_DIR_IN1, PIN_DIR_IN2, 0);
  motor(PIN_PWM_ESQ, PIN_ESQ_IN1, PIN_ESQ_IN2, 0);
}

float lerDistanciaCm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 30000);
  if (dur == 0) return -1;      // sem eco
  return dur * 0.0343 / 2.0;
}

void menu() {
  Serial.println();
  Serial.println("===== TESTE DE MONTAGEM =====");
  Serial.println("1 - Roda ESQUERDA (frente 2s, ré 2s)");
  Serial.println("2 - Roda DIREITA  (frente 2s, ré 2s)");
  Serial.println("3 - Ambas para frente 2s");
  Serial.println("4 - Servo (centro, direita, esquerda, centro)");
  Serial.println("5 - Sensor ultrassônico (20 leituras)");
  Serial.println("6 - Calibrar o BRAÇO ESQUERDO");
  Serial.println("7 - Calibrar o BRAÇO DIREITO");
  Serial.println("Digite o número:");
}

void testeMotor(const char* nome, const char* flag, int pwm, int in1, int in2) {
  Serial.printf("ESPERADO: roda %s gira PARA FRENTE...\n", nome);
  motor(pwm, in1, in2, PWM_TESTE);  delay(2000);
  pararTudo(); delay(500);
  Serial.printf("ESPERADO: roda %s gira PARA TRÁS...\n", nome);
  motor(pwm, in1, in2, -PWM_TESTE); delay(2000);
  pararTudo();
  Serial.println("Se girou a roda errada -> canais A/B trocados (avise para ajustar os pinos).");
  Serial.printf("Se girou ao contrário -> no programa principal mude %s para true.\n", flag);
  Serial.println("Se nada aconteceu -> verifique STBY (D4), VM (5V), VCC (3,3V) e GND comum.");
}

void calibrarBraco(const char* lado, int pino) {
  Serial.printf("CALIBRAR BRAÇO %s: o servo foi para 90°.\n", lado);
  escreverServo(pino, 90);
  Serial.println("1) Solte o parafuso, encaixe o braço esticado para a FRENTE (horizontal) e aperte.");
  Serial.println("2) Digite um ângulo (0-180) e Enter para testar.");
  Serial.println("   Mude de 10 em 10 graus para o braço não bater em nada.");
  Serial.println("3) Digite -1 para sair.");
  while (true) {
    while (!Serial.available()) delay(10);
    int ang = Serial.parseInt();
    while (Serial.available()) Serial.read();
    if (ang < 0) break;
    ang = constrain(ang, 0, 180);
    escreverServo(pino, ang);
    Serial.printf("  braço %s em %d°\n", lado, ang);
  }
  escreverServo(pino, 90);
  Serial.printf("Anote BRACO_%s_FRENTE e BRACO_%s_CIMA no programa principal.\n", lado, lado);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_DIR_IN1, OUTPUT); pinMode(PIN_DIR_IN2, OUTPUT);
  pinMode(PIN_ESQ_IN1, OUTPUT); pinMode(PIN_ESQ_IN2, OUTPUT);
  ledcAttachChannel(PIN_PWM_DIR, PWM_FREQ, PWM_RES, 0);
  ledcAttachChannel(PIN_PWM_ESQ, PWM_FREQ, PWM_RES, 1);
  digitalWrite(PIN_STBY, HIGH);   // TB6612 só funciona com STBY em HIGH

  // canais explícitos: motores no timer 0, servo no timer 2 (frequências diferentes)
  ledcAttachChannel(PIN_SERVO, SERVO_FREQ, SERVO_RES, 4);
  servoAngulo(90);
  ledcAttachChannel(PIN_BRACO_ESQ, SERVO_FREQ, SERVO_RES, 5);
  ledcAttachChannel(PIN_BRACO_DIR, SERVO_FREQ, SERVO_RES, 6);

  pararTudo();
  delay(500);
  menu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == '\n' || c == '\r') return;

  switch (c) {
    case '1': testeMotor("ESQUERDA", "INVERTE_ESQ", PIN_PWM_ESQ, PIN_ESQ_IN1, PIN_ESQ_IN2); break;
    case '2': testeMotor("DIREITA",  "INVERTE_DIR", PIN_PWM_DIR, PIN_DIR_IN1, PIN_DIR_IN2); break;
    case '3':
      Serial.println("ESPERADO: as duas rodas giram para frente.");
      motor(PIN_PWM_ESQ, PIN_ESQ_IN1, PIN_ESQ_IN2, PWM_TESTE);
      motor(PIN_PWM_DIR, PIN_DIR_IN1, PIN_DIR_IN2, PWM_TESTE);
      delay(2000); pararTudo();
      Serial.println("Se o ESP32 reiniciar aqui -> queda de tensão: ponha um capacitor (470-1000uF) no VM.");
      break;
    case '4':
      Serial.println("ESPERADO: sensor olha frente, DIREITA do robô, ESQUERDA do robô, frente.");
      servoAngulo(90);  delay(700);
      servoAngulo(20);  delay(700);
      servoAngulo(160); delay(700);
      servoAngulo(90);  delay(700);
      Serial.println("Se direita/esquerda saíram trocadas -> troque SERVO_DIREITA e SERVO_ESQUERDA no programa principal.");
      break;
    case '5':
      Serial.println("ESPERADO: valores coerentes. Aproxime e afaste a mão (5-100 cm).");
      for (int i = 0; i < 20; i++) {
        float d = lerDistanciaCm();
        if (d < 0) Serial.println("  sem eco (nada à frente, ou ECHO/TRIG/divisor com problema)");
        else       Serial.printf("  %.1f cm\n", d);
        delay(250);
      }
      Serial.println("Se der sempre 'sem eco' -> confira TRIG (D5), ECHO (D33), VCC do sensor em 5V e o divisor.");
      break;
    case '6': calibrarBraco("ESQ", PIN_BRACO_ESQ); break;
    case '7': calibrarBraco("DIR", PIN_BRACO_DIR); break;
    default: Serial.println("Opção inválida.");
  }
  menu();
}
