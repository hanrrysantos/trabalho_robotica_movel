/*
  ROBÔ DESVIADOR DE OBSTÁCULOS COM LÓGICA FUZZY
  Placa: ESP32 DevKit (Arduino-ESP32 core 3.x)
  Hardware: HC-SR04 sobre servo SG90, driver TB6612FNG, 2 motores TT
  Nenhuma biblioteca extra: o servo é controlado direto pelo PWM (LEDC) do ESP32

  Funcionamento (máquina de estados):
    AVANÇAR  -> lê a distância à frente; o controlador FUZZY define a velocidade
                (longe = rápido, perto = devagar, muito perto = parar).
    ESCANEAR -> quando a velocidade fuzzy cai abaixo do limiar, o robô freia
                e o servo olha para a direita e para a esquerda.
    DESVIAR  -> gira para o lado com mais espaço livre; se os dois lados
                estiverem bloqueados, dá ré e gira ~180°.

  Controlador fuzzy (tipo Mamdani):
    Entrada : distância frontal (cm)  -> MUITO_PERTO, PERTO, MEDIO, LONGE
    Saída   : velocidade (0-100 %)    -> PARAR, LENTA, MEDIA, RAPIDA
    Regras  : SE distância é MUITO_PERTO ENTÃO velocidade é PARAR
              SE distância é PERTO       ENTÃO velocidade é LENTA
              SE distância é MEDIO       ENTÃO velocidade é MEDIA
              SE distância é LONGE       ENTÃO velocidade é RAPIDA
    Inferência: min (implicação) / max (agregação)
    Defuzzificação: centroide

  Braço (servo SG90 no lado ESQUERDO):
    Ao ligar        -> acena "tchau"
    Andando         -> balança no ritmo da velocidade fuzzy
    Muito perto     -> levanta o braço (susto)
    Virar à esquerda-> acena antes, sinalizando o lado do braço
    Beco sem saída  -> "dá de ombros" (sobe e desce 2x)
*/
// =================== PINOS (montagem conferida) ===================
// Direita/esquerda = do ponto de vista do ROBÔ (olhando para onde o sensor aponta)
// Sensor HC-SR04
const int PIN_TRIG  = 5;
const int PIN_ECHO  = 33;   // via divisor 1k / 2k
// Servo SG90
const int PIN_SERVO = 13;
// Servo do braço esquerdo
const int PIN_BRACO = 16;
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

// =================== AJUSTES (calibrar no robô) ===================
const bool INVERTE_ESQ = false;   // true se a roda esquerda andar ao contrário
const bool INVERTE_DIR = false;

const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
const int PWM_MIN  = 110;   // menor PWM que ainda faz o robô andar no chão
const int PWM_MAX  = 200;
const int PWM_GIRO = 170;

const int SERVO_CENTRO   = 90;
const int SERVO_DIREITA  = 20;
const int SERVO_ESQUERDA = 160;

// Braço: use a opção 6 do teste_montagem para descobrir estes ângulos
const int BRACO_FRENTE = 90;    // braço esticado para a FRENTE (horizontal)
const int BRACO_CIMA   = 0;     // braço apontando para CIMA (calibrado neste robô)

const float VEL_LIMIAR     = 20.0;  // % : abaixo disso considera obstáculo
const float DIST_LIVRE     = 30.0;  // cm: lado com menos que isso está bloqueado
const float DIST_RE        = 12.0;  // cm: tão perto que precisa dar ré antes
const int   TEMPO_GIRO_90  = 350;   // ms para girar ~90° (calibrar!)
const int   TEMPO_RE       = 350;   // ms de ré

const bool DEBUG = true;

// =================== LÓGICA FUZZY ===================
struct Conjunto {
  const char* nome;
  float a, b, c, d;   // trapézio; triângulo quando b == c
};

// Entrada: distância (cm)
Conjunto entradaDist[] = {
  {"MUITO_PERTO",  0,   0,  15,  25},
  {"PERTO",       15,  30,  30,  45},
  {"MEDIO",       35,  60,  60,  90},
  {"LONGE",       75, 120, 500, 500},
};

// Saída: velocidade (%)
Conjunto saidaVel[] = {
  {"PARAR",   0,  0,   0,  15},
  {"LENTA",  10, 30,  30,  50},
  {"MEDIA",  40, 60,  60,  80},
  {"RAPIDA", 70, 90, 100, 100},
};

// Base de regras: entrada i -> saída regras[i]
const int regras[] = {0, 1, 2, 3};
const int N_REGRAS = 4;

// Grau de pertinência num trapézio (funciona também para ombros e triângulos)
float pertinencia(float x, const Conjunto& s) {
  if (x < s.a || x > s.d) return 0.0;
  if (x >= s.b && x <= s.c) return 1.0;
  if (x < s.b) return (x - s.a) / (s.b - s.a);
  return (s.d - x) / (s.d - s.c);
}

float graus[N_REGRAS];   // guardado para o debug

float fuzzyVelocidade(float dist) {
  // 1) Fuzzificação + ativação das regras
  for (int i = 0; i < N_REGRAS; i++) graus[i] = pertinencia(dist, entradaDist[i]);

  // 2) Implicação (min), agregação (max) e 3) defuzzificação por centroide
  float num = 0, den = 0;
  for (float x = 0; x <= 100; x += 1.0) {
    float mu = 0;
    for (int r = 0; r < N_REGRAS; r++) {
      float m = min(graus[r], pertinencia(x, saidaVel[regras[r]]));
      mu = max(mu, m);
    }
    num += x * mu;
    den += mu;
  }
  return (den == 0) ? 0 : num / den;
}

// =================== SENSOR ===================
// Servo SG90 via PWM do ESP32: 50 Hz, pulso de 500 us (0°) a 2400 us (180°)
const int SERVO_FREQ = 50;
const int SERVO_RES  = 14;   // 0 a 16383

void escreverServo(int pino, int angulo) {
  angulo = constrain(angulo, 0, 180);
  long pulsoUs = map(angulo, 0, 180, 500, 2400);
  ledcWrite(pino, pulsoUs * 16383L / 20000L);
}

void servoAngulo(int angulo) { escreverServo(PIN_SERVO, angulo); }

// =================== BRAÇO ===================
// Posição do braço como fração: 0 = esticado para frente, 1 = apontando para cima.
// Fica limitado a esse intervalo para o braço nunca bater no chão nem na roda.
void bracoPos(float frac) {
  frac = constrain(frac, 0.0, 1.0);
  escreverServo(PIN_BRACO, BRACO_FRENTE + frac * (BRACO_CIMA - BRACO_FRENTE));
}

void acenar(int vezes) {
  for (int i = 0; i < vezes; i++) {
    bracoPos(1.0); delay(250);
    bracoPos(0.7); delay(250);
  }
  bracoPos(0);
}

void darDeOmbros() {
  for (int i = 0; i < 2; i++) {
    bracoPos(0.6); delay(200);
    bracoPos(0.0); delay(200);
  }
}

// Balanço ao andar: mais rápido e mais amplo quanto maior a velocidade fuzzy
float faseBraco = 0;
unsigned long ultimoBraco = 0;

void bracoAndando(float vel) {
  unsigned long agora = millis();
  float dt = (agora - ultimoBraco) / 1000.0;
  ultimoBraco = agora;
  if (dt > 0.5) dt = 0.5;
  float freq = 0.4 + 1.2 * vel / 100.0;    // Hz
  float amp  = 0.10 + 0.40 * vel / 100.0;  // fração do caminho até "cima"
  faseBraco += 2 * PI * freq * dt;
  bracoPos(amp * (0.5 - 0.5 * cos(faseBraco)));
}

float lerDistanciaCm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 30000);
  if (dur == 0) return 400;          // sem eco = nada à frente
  return dur * 0.0343 / 2.0;
}

// Mediana de 3 leituras: elimina leituras falsas isoladas
float distanciaFiltrada() {
  float v[3];
  for (int i = 0; i < 3; i++) { v[i] = lerDistanciaCm(); delay(20); }
  if (v[0] > v[1]) { float t = v[0]; v[0] = v[1]; v[1] = t; }
  if (v[1] > v[2]) { float t = v[1]; v[1] = v[2]; v[2] = t; }
  if (v[0] > v[1]) { float t = v[0]; v[0] = v[1]; v[1] = t; }
  return v[1];
}

float medirNoAngulo(int angulo) {
  servoAngulo(angulo);
  delay(350);                        // tempo para o servo chegar
  return distanciaFiltrada();
}

// =================== MOTORES (TB6612FNG) ===================
void motor(int pwmPin, int in1, int in2, int vel, bool inverte) {
  if (inverte) vel = -vel;
  vel = constrain(vel, -255, 255);
  digitalWrite(in1, vel > 0 ? HIGH : LOW);
  digitalWrite(in2, vel < 0 ? HIGH : LOW);
  ledcWrite(pwmPin, abs(vel));
}

void mover(int esq, int dir) {
  motor(PIN_PWM_ESQ, PIN_ESQ_IN1, PIN_ESQ_IN2, esq, INVERTE_ESQ);
  motor(PIN_PWM_DIR, PIN_DIR_IN1, PIN_DIR_IN2, dir, INVERTE_DIR);
}

void frear() {   // freio curto do TB6612: IN1 = IN2 = HIGH
  digitalWrite(PIN_ESQ_IN1, HIGH); digitalWrite(PIN_ESQ_IN2, HIGH);
  digitalWrite(PIN_DIR_IN1, HIGH); digitalWrite(PIN_DIR_IN2, HIGH);
  ledcWrite(PIN_PWM_ESQ, 255);     ledcWrite(PIN_PWM_DIR, 255);
  delay(150);
  mover(0, 0);
}

void girarEsquerda(int ms) { mover(-PWM_GIRO,  PWM_GIRO); delay(ms); frear(); }
void girarDireita(int ms)  { mover( PWM_GIRO, -PWM_GIRO); delay(ms); frear(); }
void re(int ms)            { mover(-PWM_GIRO, -PWM_GIRO); delay(ms); frear(); }

// =================== PROGRAMA ===================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_DIR_IN1, OUTPUT); pinMode(PIN_DIR_IN2, OUTPUT);
  pinMode(PIN_ESQ_IN1, OUTPUT); pinMode(PIN_ESQ_IN2, OUTPUT);
  ledcAttachChannel(PIN_PWM_DIR, PWM_FREQ, PWM_RES, 0);
  ledcAttachChannel(PIN_PWM_ESQ, PWM_FREQ, PWM_RES, 1);
  digitalWrite(PIN_STBY, HIGH);

  // canais explícitos: motores no timer 0, servo no timer 2 (frequências diferentes)
  ledcAttachChannel(PIN_SERVO, SERVO_FREQ, SERVO_RES, 4);
  servoAngulo(SERVO_CENTRO);
  // braço no mesmo timer do servo da cabeça (mesma frequência)
  ledcAttachChannel(PIN_BRACO, SERVO_FREQ, SERVO_RES, 5);

  mover(0, 0);
  acenar(4);     // ~2 s acenando: tempo para colocar o robô no chão
  ultimoBraco = millis();
}

void loop() {
  // ---------- AVANÇAR ----------
  float d   = distanciaFiltrada();
  float vel = fuzzyVelocidade(d);

  if (DEBUG) {
    Serial.printf("d=%6.1f cm | MP=%.2f P=%.2f M=%.2f L=%.2f | vel=%5.1f%%\n",
                  d, graus[0], graus[1], graus[2], graus[3], vel);
  }

  if (vel >= VEL_LIMIAR) {
    int pwm = PWM_MIN + (vel - VEL_LIMIAR) * (PWM_MAX - PWM_MIN) / (100.0 - VEL_LIMIAR);
    mover(pwm, pwm);
    bracoAndando(vel);
    return;
  }

  // ---------- ESCANEAR ----------
  frear();
  if (d < DIST_RE) {
    bracoPos(1.0);           // susto: braço para cima
    re(TEMPO_RE);
    delay(200);
  }
  bracoPos(0);

  float dDir = medirNoAngulo(SERVO_DIREITA);
  float dEsq = medirNoAngulo(SERVO_ESQUERDA);
  servoAngulo(SERVO_CENTRO);
  delay(250);

  if (DEBUG) Serial.printf(">> Obstáculo! esquerda=%.1f cm  direita=%.1f cm\n", dEsq, dDir);

  // ---------- DESVIAR ----------
  if (dEsq < DIST_LIVRE && dDir < DIST_LIVRE) {
    if (DEBUG) Serial.println(">> Beco sem saída: ré + meia-volta");
    darDeOmbros();
    re(TEMPO_RE);
    girarDireita(2 * TEMPO_GIRO_90);
  } else if (dEsq > dDir) {
    if (DEBUG) Serial.println(">> Virando à esquerda");
    acenar(2);               // sinaliza que vai para o lado do braço
    girarEsquerda(TEMPO_GIRO_90);
  } else {
    if (DEBUG) Serial.println(">> Virando à direita");
    girarDireita(TEMPO_GIRO_90);
  }
}
