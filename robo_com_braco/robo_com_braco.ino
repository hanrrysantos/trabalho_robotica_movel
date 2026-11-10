/*
  ROBÔ DESVIADOR DE OBSTÁCULOS COM LÓGICA FUZZY
  Placa: ESP32 DevKit (Arduino-ESP32 core 3.x)
  Hardware: HC-SR04 sobre servo SG90, driver TB6612FNG, 2 motores TT
  Nenhuma biblioteca extra: o servo é controlado direto pelo PWM (LEDC) do ESP32

  Funcionamento (máquina de estados):
    AVANÇAR  -> enquanto anda, a cabeça VARRE a frente (centro, um pouco à direita,
                centro, um pouco à esquerda) para cobrir os pontos cegos das
                quinas. A menor distância que está no caminho do robô vai para o
                controlador FUZZY, que define a velocidade
                (longe = rápido, perto = devagar, muito perto = parar).
                Se a distância não muda por um tempo com o robô "andando", ele
                está PRESO em algo que o sensor não viu: dá ré e desvia.
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

  Braços (2 servos SG90, um de cada lado):
    Ao ligar          -> os dois acenam "tchau" (alternados)
    Andando           -> balançam alternados, como numa caminhada,
                         no ritmo da velocidade fuzzy
    Muito perto       -> os dois sobem de uma vez (susto) e o robô dá ré
    Preso             -> "pedem socorro": os dois agitam no alto, depois ré
    Olhando um lado   -> o braço daquele lado aponta para lá
    Virar             -> o braço do lado da curva acena antes (seta)
    Beco sem saída    -> os dois "dão de ombros" (sobem e descem 2x)

  Controle pelo celular (Wi-Fi, sem app):
    1. Conecte o celular na rede "wally 2.0" (senha 12345678).
    2. Abra o navegador em http://192.168.4.1
    3. LIGAR / PAUSAR, modo Automático ou Manual (setas) e painel da
       lógica fuzzy ao vivo. O robô liga PAUSADO e só anda pelo celular.
*/
#include <WiFi.h>
#include <WebServer.h>

// Tipo fuzzy declarado ANTES dos protótipos automáticos do Arduino IDE.
struct Conjunto {
  const char* nome;
  float a, b, c, d;
};

// Declaração explícita para evitar: 'Conjunto' does not name a type.
float pertinencia(float x, const Conjunto& s);

void espera(int ms);   // definida mais abaixo; atende o celular enquanto espera
// =================== PINOS (montagem conferida) ===================
// Direita/esquerda = do ponto de vista do ROBÔ (olhando para onde o sensor aponta)
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

// =================== AJUSTES (calibrar no robô) ===================
const bool INVERTE_ESQ = false;   // true se a roda esquerda andar ao contrário
const bool INVERTE_DIR = false;

// Correção para andar reto: multiplica o PWM de cada roda.
// Teste no chão: esquerda 170 e direita 230 andou reto -> 170/230 = 0,74.
// Se puxar para a DIREITA, diminua FATOR_ESQ; se puxar para a ESQUERDA, aumente.
const float FATOR_ESQ = 0.85;
const float FATOR_DIR = 1.00;

const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
const int PWM_MIN  = 110;   // menor PWM que ainda faz o robô andar no chão
const int PWM_MAX  = 200;
const int PWM_GIRO = 170;

const int SERVO_CENTRO   = 90;
const int SERVO_DIREITA  = 20;
const int SERVO_ESQUERDA = 160;

// Braços: use as opções 6 (esquerdo) e 7 (direito) do teste_montagem
const int BRACO_ESQ_FRENTE = 90;   // braço esquerdo esticado para a FRENTE
const int BRACO_ESQ_CIMA   = 0;    // braço esquerdo apontando para CIMA (calibrado)
const int BRACO_DIR_FRENTE = 90;   // braço direito esticado para a FRENTE
const int BRACO_DIR_CIMA   = 180;  // braço direito apontando para CIMA (CALIBRAR: 180 ou 0)

const float VEL_LIMIAR     = 20.0;  // % : abaixo disso considera obstáculo
const float DIST_LIVRE     = 30.0;  // cm: lado com menos que isso está bloqueado
const float DIST_RE        = 12.0;  // cm: tão perto que precisa dar ré antes
const int   TEMPO_GIRO_90  = 350;   // ms para girar ~90° (calibrar!)
const int   TEMPO_RE       = 350;   // ms de ré

// Varredura enquanto anda (cobre as quinas da frente)
const int   VARREDURA_ABERTURA = 40;    // graus para cada lado do centro (era 25)
const int   TEMPO_PASSO_SERVO  = 100;   // ms para a cabeça chegar no próximo ângulo (era 70)
const float MEIA_LARGURA       = 15.0;  // cm: metade da largura do robô + folga.
                                        // Objetos mais para o lado que isso não estão no caminho.

// Detecção de robô preso
const int   TEMPO_PRESO    = 2500;  // ms andando sem a distância mudar = preso
const float VARIACAO_PRESO = 4.0;   // cm: mudança mínima que indica que está andando

const bool DEBUG = true;

// Wi-Fi e controle pelo celular
const char* WIFI_NOME  = "wally 2.0";
const char* WIFI_SENHA = "12345678";   // mínimo de 8 caracteres
const int   PWM_MANUAL = 180;          // velocidade para frente/ré no modo manual
const int   TEMPO_SEGURANCA_MANUAL = 600;  // ms sem comando do celular = para
const float DIST_ANTICOLISAO = 15.0;   // cm: no manual, bloqueia "frente" se estiver mais perto

// =================== ESTADO DO ROBÔ ===================
WebServer server(80);
bool ligado = false;              // começa pausado
bool modoManual = false;
bool reiniciarAuto = true;        // recomeçar varredura/preso ao (re)entrar no automático
const char* estado = "PAUSADO";
const char* comandoManual = "parar";
unsigned long ultimoComando = 0;
float telDist = 400, telVel = 0;  // telemetria para o painel
int   telPwm = 0;

// =================== LÓGICA FUZZY ===================

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

// =================== BRAÇOS ===================
// Posição de cada braço como fração: 0 = esticado para frente, 1 = apontando para cima.
// Fica limitado a esse intervalo para o braço nunca bater no chão nem na roda.
void bracoEsq(float frac) {
  frac = constrain(frac, 0.0, 1.0);
  escreverServo(PIN_BRACO_ESQ, BRACO_ESQ_FRENTE + frac * (BRACO_ESQ_CIMA - BRACO_ESQ_FRENTE));
}

void bracoDir(float frac) {
  frac = constrain(frac, 0.0, 1.0);
  escreverServo(PIN_BRACO_DIR, BRACO_DIR_FRENTE + frac * (BRACO_DIR_CIMA - BRACO_DIR_FRENTE));
}

void bracos(float esq, float dir) { bracoEsq(esq); bracoDir(dir); }

// Acena com um braço (seta antes de virar)
void acenarEsq(int vezes) {
  for (int i = 0; i < vezes; i++) { bracoEsq(1.0); espera(250); bracoEsq(0.7); espera(250); }
  bracoEsq(0);
}

void acenarDir(int vezes) {
  for (int i = 0; i < vezes; i++) { bracoDir(1.0); espera(250); bracoDir(0.7); espera(250); }
  bracoDir(0);
}

// Os dois acenam, alternados (um sobe enquanto o outro desce)
void acenarDois(int vezes) {
  for (int i = 0; i < vezes; i++) {
    bracos(1.0, 0.7); espera(250);
    bracos(0.7, 1.0); espera(250);
  }
  bracos(0, 0);
}

void darDeOmbros() {
  for (int i = 0; i < 2; i++) {
    bracos(0.6, 0.6); espera(200);
    bracos(0.0, 0.0); espera(200);
  }
}

// Preso: os dois braços no alto, agitando rápido
void pedirSocorro() {
  for (int i = 0; i < 4; i++) {
    bracos(1.0, 0.8); espera(120);
    bracos(0.8, 1.0); espera(120);
  }
  bracos(0, 0);
}

// Balanço ao andar: os braços alternam, como numa caminhada.
// Quanto maior a velocidade fuzzy, mais rápido e mais amplo o balanço.
float faseBraco = 0;
unsigned long ultimoBraco = 0;
float velBracos = 0;      // velocidade usada na animação (0 = braços parados)

void bracosAndando(float vel) {
  unsigned long agora = millis();
  float dt = (agora - ultimoBraco) / 1000.0;
  ultimoBraco = agora;
  if (dt > 0.2) dt = 0.2;
  float freq = 0.4 + 0.8 * vel / 100.0;    // Hz
  float amp  = 0.10 + 0.40 * vel / 100.0;  // fração do caminho até "cima"
  faseBraco += 2 * PI * freq * dt;
  float onda = 0.5 - 0.5 * cos(faseBraco); // 0 a 1
  bracos(amp * onda, amp * (1.0 - onda));  // em oposição de fase
}

float lerDistanciaCm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  // timeout de 12 ms = alcance de ~2 m (suficiente, e deixa a varredura mais rápida)
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 12000);
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
  espera(350);                        // tempo para o servo chegar
  return distanciaFiltrada();
}

// Espera que continua atendendo o celular e, se o robô estiver andando,
// mantém os braços balançando. Usada no lugar de delay() no programa todo.
void espera(int ms) {
  unsigned long inicio = millis();
  while (millis() - inicio < (unsigned long)ms) {
    server.handleClient();
    if (velBracos > 0) bracosAndando(velBracos);
    delay(10);
  }
}

// =================== VARREDURA ===================
// Sequência da cabeça enquanto anda: centro, direita, centro, esquerda.
// O centro é lido a cada 2 passos porque é a direção mais importante.
// (ângulo menor = direita do robô, ângulo maior = esquerda)
const int SEQ_VARREDURA[4] = {
  SERVO_CENTRO,
  SERVO_CENTRO - VARREDURA_ABERTURA,
  SERVO_CENTRO,
  SERVO_CENTRO + VARREDURA_ABERTURA
};
int   passoVarredura = 0;
float distCentro = 400, distDirFrente = 400, distEsqFrente = 400;

// Converte a leitura inclinada em "distância à frente no caminho do robô".
// Um objeto lido a d cm num ângulo θ do centro está d·cos(θ) à frente e
// d·sin(θ) para o lado. Se estiver mais para o lado que MEIA_LARGURA,
// o robô passa por ele e a leitura é ignorada.
float distanciaNoCaminho(float d, int grausDoCentro) {
  float rad = abs(grausDoCentro) * PI / 180.0;
  if (d * sin(rad) > MEIA_LARGURA) return 400;
  return d * cos(rad);
}

// Dá um passo da varredura e devolve a menor distância no caminho do robô
float passoDeVarredura() {
  int angulo = SEQ_VARREDURA[passoVarredura];
  passoVarredura = (passoVarredura + 1) % 4;

  servoAngulo(angulo);
  espera(TEMPO_PASSO_SERVO);
  float d = distanciaFiltrada();

  if (angulo == SERVO_CENTRO)      distCentro    = d;
  else if (angulo < SERVO_CENTRO)  distDirFrente = d;
  else                             distEsqFrente = d;

  float dEfetiva = distCentro;
  dEfetiva = min(dEfetiva, distanciaNoCaminho(distDirFrente, VARREDURA_ABERTURA));
  dEfetiva = min(dEfetiva, distanciaNoCaminho(distEsqFrente, VARREDURA_ABERTURA));
  return dEfetiva;
}

void reiniciarVarredura() {
  passoVarredura = 0;
  distCentro = distDirFrente = distEsqFrente = 400;
}

// =================== DETECÇÃO DE "PRESO" ===================
// Andando, a distância medida à frente muda o tempo todo. Se ela fica
// parada por TEMPO_PRESO ms, as rodas estão girando mas o robô não sai
// do lugar: encostou em algo que o sensor não viu.
float distReferencia = -1;
unsigned long tempoReferencia = 0;

void reiniciarPreso() {
  distReferencia = -1;
  tempoReferencia = millis();
}

bool verificarPreso() {
  if (distCentro >= 200) {          // sem eco: não dá para saber, não acusa
    reiniciarPreso();
    return false;
  }
  if (distReferencia < 0 || abs(distCentro - distReferencia) > VARIACAO_PRESO) {
    distReferencia = distCentro;
    tempoReferencia = millis();
    return false;
  }
  return (millis() - tempoReferencia) > TEMPO_PRESO;
}

// =================== MOTORES (TB6612FNG) ===================
void motor(int pwmPin, int in1, int in2, int vel, bool inverte) {
  if (inverte) vel = -vel;
  vel = constrain(vel, -255, 255);
  digitalWrite(in1, vel > 0 ? HIGH : LOW);
  digitalWrite(in2, vel < 0 ? HIGH : LOW);
  ledcWrite(pwmPin, abs(vel));
}

// Todo movimento passa por aqui, então a correção vale para andar, ré e giros
void mover(int esq, int dir) {
  motor(PIN_PWM_ESQ, PIN_ESQ_IN1, PIN_ESQ_IN2, (int)(esq * FATOR_ESQ), INVERTE_ESQ);
  motor(PIN_PWM_DIR, PIN_DIR_IN1, PIN_DIR_IN2, (int)(dir * FATOR_DIR), INVERTE_DIR);
}

void frear() {   // freio curto do TB6612: IN1 = IN2 = HIGH
  digitalWrite(PIN_ESQ_IN1, HIGH); digitalWrite(PIN_ESQ_IN2, HIGH);
  digitalWrite(PIN_DIR_IN1, HIGH); digitalWrite(PIN_DIR_IN2, HIGH);
  ledcWrite(PIN_PWM_ESQ, 255);     ledcWrite(PIN_PWM_DIR, 255);
  espera(150);
  mover(0, 0);
}

void girarEsquerda(int ms) { mover(-PWM_GIRO,  PWM_GIRO); espera(ms); frear(); }
void girarDireita(int ms)  { mover( PWM_GIRO, -PWM_GIRO); espera(ms); frear(); }
void re(int ms)            { mover(-PWM_GIRO, -PWM_GIRO); espera(ms); frear(); }

// =================== PÁGINA DO CELULAR ===================
const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Wally 2.0</title>
<style>
:root{--bg:#16130f;--card:#231e18;--txt:#f3ece4;--sub:#a99c8d;--lar:#f28a1e;--ok:#3fbf6f;--stop:#e5534b}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--txt);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:16px;max-width:480px;margin:auto}
h1{font-size:22px;margin:4px 0 2px}.sub{color:var(--sub);font-size:13px;margin-bottom:14px}
.card{background:var(--card);border-radius:14px;padding:14px;margin-bottom:12px}
.estado{display:inline-block;padding:4px 10px;border-radius:99px;background:#3a3129;font-weight:600;font-size:13px}
.btn{width:100%;border:0;border-radius:12px;padding:18px;font-size:20px;font-weight:700;color:#fff;margin-top:10px}
.ligar{background:var(--ok)}.pausar{background:var(--stop)}
.seg{display:flex;background:#3a3129;border-radius:10px;padding:3px;margin-top:10px}
.seg button{flex:1;border:0;background:none;color:var(--sub);padding:10px;border-radius:8px;font-size:15px;font-weight:600}
.seg button.on{background:var(--lar);color:#1a120a}
.dist{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;text-align:center}
.dist div{background:#2e2720;border-radius:10px;padding:8px 4px}
.dist b{display:block;font-size:20px}.dist span{font-size:11px;color:var(--sub)}
.lin{display:flex;align-items:center;gap:8px;margin-top:8px;font-size:13px}
.lin .n{width:96px;color:var(--sub)}.lin .v{width:42px;text-align:right;font-variant-numeric:tabular-nums}
.bar{flex:1;height:12px;background:#2e2720;border-radius:6px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:var(--lar);transition:width .25s}
.vel i{background:var(--ok)}
h2{font-size:14px;margin:0 0 8px;color:var(--sub);font-weight:600}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;touch-action:none;user-select:none;-webkit-user-select:none}
.pad button{height:72px;border:0;border-radius:14px;background:#3a3129;color:var(--txt);font-size:30px}
.pad button:active,.pad button.pr{background:var(--lar);color:#1a120a}
.pad.off{opacity:.35;pointer-events:none}
.aviso{font-size:12px;color:var(--sub);margin-top:8px}
</style></head><body>
<h1>Wally 2.0</h1><div class="sub">Robô desviador de obstáculos · lógica fuzzy</div>

<div class="card">
  <span class="estado" id="estado">conectando…</span>
  <button class="btn ligar" id="bLigar" onclick="alternar()">LIGAR</button>
  <div class="seg"><button id="mA" onclick="modo('auto')">Automático</button><button id="mM" onclick="modo('manual')">Manual</button></div>
</div>

<div class="card">
  <h2>Sensor (varredura)</h2>
  <div class="dist">
    <div><b id="dE">–</b><span>esquerda</span></div>
    <div><b id="dC">–</b><span>centro</span></div>
    <div><b id="dD">–</b><span>direita</span></div>
  </div>
  <div class="lin" style="margin-top:10px"><span style="color:var(--sub)">Distância usada pelo fuzzy:</span><b id="d">–</b></div>
</div>

<div class="card">
  <h2>Lógica fuzzy</h2>
  <div class="lin"><span class="n">Muito perto</span><div class="bar"><i id="bMP"></i></div><span class="v" id="vMP">0</span></div>
  <div class="lin"><span class="n">Perto</span><div class="bar"><i id="bP"></i></div><span class="v" id="vP">0</span></div>
  <div class="lin"><span class="n">Médio</span><div class="bar"><i id="bM"></i></div><span class="v" id="vM">0</span></div>
  <div class="lin"><span class="n">Longe</span><div class="bar"><i id="bL"></i></div><span class="v" id="vL">0</span></div>
  <div class="lin" style="margin-top:14px"><span class="n"><b style="color:var(--txt)">Velocidade</b></span><div class="bar vel"><i id="bV"></i></div><span class="v" id="vV">0%</span></div>
</div>

<div class="card">
  <h2>Controle manual</h2>
  <div class="pad off" id="pad">
    <span></span><button data-c="frente">▲</button><span></span>
    <button data-c="esquerda">◀</button><span></span><button data-c="direita">▶</button>
    <span></span><button data-c="re">▼</button><span></span>
  </div>
  <div class="aviso" id="avisoPad">Ligue o robô e escolha o modo Manual. Segure a seta para andar.</div>
</div>

<script>
let st={ligado:0,manual:0},tmr=null;
const $=id=>document.getElementById(id);
const cm=v=>v>=200?'livre':Math.round(v)+' cm';
function get(u){return fetch(u).catch(()=>{});}
function alternar(){get('/ligar?v='+(st.ligado?0:1));}
function modo(m){get('/modo?m='+m);}
function barra(id,v){$('b'+id).style.width=(v*100)+'%';$('v'+id).textContent=v.toFixed(2);}
async function atualizar(){
  try{
    const r=await fetch('/estado');st=await r.json();
    $('estado').textContent=st.estado;
    const b=$('bLigar');b.textContent=st.ligado?'PAUSAR':'LIGAR';b.className='btn '+(st.ligado?'pausar':'ligar');
    $('mA').className=st.manual?'':'on';$('mM').className=st.manual?'on':'';
    $('pad').className='pad'+(st.ligado&&st.manual?'':' off');
    $('dE').textContent=cm(st.dE);$('dC').textContent=cm(st.dC);$('dD').textContent=cm(st.dD);$('d').textContent=cm(st.d);
    barra('MP',st.mp);barra('P',st.p);barra('M',st.m);barra('L',st.l);
    $('bV').style.width=st.vel+'%';$('vV').textContent=Math.round(st.vel)+'%';
  }catch(e){$('estado').textContent='sem conexão';}
}
setInterval(atualizar,300);atualizar();
document.querySelectorAll('.pad button').forEach(bt=>{
  const c=bt.dataset.c;
  const ini=e=>{e.preventDefault();bt.classList.add('pr');get('/mover?c='+c);clearInterval(tmr);tmr=setInterval(()=>get('/mover?c='+c),200);};
  const fim=()=>{bt.classList.remove('pr');clearInterval(tmr);tmr=null;get('/mover?c=parar');};
  bt.addEventListener('pointerdown',ini);
  ['pointerup','pointerleave','pointercancel'].forEach(ev=>bt.addEventListener(ev,fim));
});
</script></body></html>)rawliteral";

void enviarPagina() { server.send_P(200, "text/html; charset=utf-8", PAGINA); }

void enviarEstado() {
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"ligado\":%d,\"manual\":%d,\"estado\":\"%s\",\"dE\":%.0f,\"dC\":%.0f,\"dD\":%.0f,"
    "\"d\":%.0f,\"mp\":%.2f,\"p\":%.2f,\"m\":%.2f,\"l\":%.2f,\"vel\":%.0f,\"pwm\":%d}",
    ligado, modoManual, estado, distEsqFrente, distCentro, distDirFrente,
    telDist, graus[0], graus[1], graus[2], graus[3], telVel, telPwm);
  server.send(200, "application/json", buf);
}

// Os comandos só mudam variáveis; quem mexe nos motores é o loop().
void receberLigar() {
  ligado = server.arg("v") == "1";
  reiniciarAuto = true;
  comandoManual = "parar";
  server.send(200, "text/plain", "ok");
}

void receberModo() {
  modoManual = server.arg("m") == "manual";
  reiniciarAuto = true;
  comandoManual = "parar";
  server.send(200, "text/plain", "ok");
}

void receberMover() {
  String c = server.arg("c");
  if      (c == "frente")   comandoManual = "frente";
  else if (c == "re")       comandoManual = "re";
  else if (c == "esquerda") comandoManual = "esquerda";
  else if (c == "direita")  comandoManual = "direita";
  else                      comandoManual = "parar";
  ultimoComando = millis();
  server.send(200, "text/plain", "ok");
}

void iniciarWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NOME, WIFI_SENHA);
  server.on("/", enviarPagina);
  server.on("/estado", enviarEstado);
  server.on("/ligar", receberLigar);
  server.on("/modo", receberModo);
  server.on("/mover", receberMover);
  server.begin();
  Serial.printf("Wi-Fi \"%s\" pronto. Abra http://%s no celular.\n",
                WIFI_NOME, WiFi.softAPIP().toString().c_str());
}

// Para tudo e deixa o robô em repouso
void pararTudo() {
  mover(0, 0);
  velBracos = 0;
  bracos(0, 0);
  servoAngulo(SERVO_CENTRO);
  telPwm = 0;
}

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
  // braços: canais 5 e 6, todos os servos a 50 Hz
  ledcAttachChannel(PIN_BRACO_ESQ, SERVO_FREQ, SERVO_RES, 5);
  ledcAttachChannel(PIN_BRACO_DIR, SERVO_FREQ, SERVO_RES, 6);

  mover(0, 0);
  iniciarWiFi();
  acenarDois(4); // acena para mostrar que ligou e está pronto
  ultimoBraco = millis();
  reiniciarVarredura();
  reiniciarPreso();
  estado = "PAUSADO";
}

void loopPausado();
void loopManual();
void loopAutomatico();

void loop() {
  server.handleClient();

  if (!ligado) {
    loopPausado();
  } else if (modoManual) {
    loopManual();
  } else {
    loopAutomatico();
  }
}

// ---------- PAUSADO ----------
// Fica parado, mas continua medindo à frente: o painel fuzzy no celular
// segue funcionando (dá para demonstrar passando a mão na frente do sensor).
void loopPausado() {
  if (strcmp(estado, "PAUSADO") != 0) pararTudo();
  estado = "PAUSADO";
  servoAngulo(SERVO_CENTRO);
  distCentro = distanciaFiltrada();
  distDirFrente = distEsqFrente = 400;
  telDist = distCentro;
  telVel  = fuzzyVelocidade(distCentro);
  espera(80);
}

// ---------- MANUAL ----------
void loopManual() {
  servoAngulo(SERVO_CENTRO);
  float d = distanciaFiltrada();
  distCentro = d;
  distDirFrente = distEsqFrente = 400;
  telDist = d;
  telVel  = fuzzyVelocidade(d);   // mostra o que o fuzzy faria

  // segurança: se o celular parar de mandar comandos (soltou a seta ou caiu o Wi-Fi), para
  if (millis() - ultimoComando > TEMPO_SEGURANCA_MANUAL) comandoManual = "parar";

  int esq = 0, dir = 0;
  estado = "MANUAL";
  if (strcmp(comandoManual, "frente") == 0) {
    if (d < DIST_ANTICOLISAO) estado = "MANUAL - BLOQUEADO";   // anticolisão
    else { esq = PWM_MANUAL; dir = PWM_MANUAL; }
  } else if (strcmp(comandoManual, "re") == 0) {
    esq = -PWM_MANUAL; dir = -PWM_MANUAL;
  } else if (strcmp(comandoManual, "esquerda") == 0) {
    esq = -PWM_GIRO; dir = PWM_GIRO;
  } else if (strcmp(comandoManual, "direita") == 0) {
    esq = PWM_GIRO; dir = -PWM_GIRO;
  }

  mover(esq, dir);
  telPwm = max(abs(esq), abs(dir));
  if (esq != 0 || dir != 0) {
    velBracos = 60;                 // braços balançam enquanto anda
  } else if (velBracos > 0) {
    velBracos = 0;
    bracos(0, 0);
  }
  espera(20);
}

// ---------- AUTOMÁTICO ----------
void loopAutomatico() {
  if (reiniciarAuto) {
    reiniciarAuto = false;
    pararTudo();
    reiniciarVarredura();
    reiniciarPreso();
    ultimoBraco = millis();
  }

  // ----- AVANÇAR (com varredura) -----
  float d = passoDeVarredura();
  if (!ligado || modoManual) return;   // pausaram ou trocaram de modo durante a varredura

  float vel = fuzzyVelocidade(d);
  bool preso = (vel >= VEL_LIMIAR) && verificarPreso();
  telDist = d;
  telVel  = vel;

  if (DEBUG) {
    Serial.printf("D=%5.1f C=%5.1f E=%5.1f -> d=%5.1f cm | MP=%.2f P=%.2f M=%.2f L=%.2f | vel=%5.1f%%\n",
                  distDirFrente, distCentro, distEsqFrente, d,
                  graus[0], graus[1], graus[2], graus[3], vel);
  }

  if (vel >= VEL_LIMIAR && !preso) {
    int pwm = PWM_MIN + (vel - VEL_LIMIAR) * (PWM_MAX - PWM_MIN) / (100.0 - VEL_LIMIAR);
    mover(pwm, pwm);
    telPwm = pwm;
    estado = "ANDANDO";
    velBracos = vel;        // braços balançam no ritmo da velocidade fuzzy
    bracosAndando(vel);
    return;
  }

  // ----- ESCANEAR -----
  frear();
  telPwm = 0;
  velBracos = 0;            // para o balanço
  if (preso) {
    estado = "PRESO";
    if (DEBUG) Serial.println(">> Preso! Dando ré.");
    pedirSocorro();
    re(TEMPO_RE * 2);
  } else if (d < DIST_RE) {
    estado = "MUITO PERTO";
    bracos(1.0, 1.0);       // susto: os dois braços para cima
    re(TEMPO_RE);
    espera(200);
  }
  bracos(0, 0);

  estado = "ESCANEANDO";
  bracos(0, 0.5);           // olhando para a direita: braço direito aponta
  float dDir = medirNoAngulo(SERVO_DIREITA);
  bracos(0.5, 0);           // olhando para a esquerda: braço esquerdo aponta
  float dEsq = medirNoAngulo(SERVO_ESQUERDA);
  bracos(0, 0);
  servoAngulo(SERVO_CENTRO);
  espera(250);

  if (DEBUG) Serial.printf(">> Obstáculo! esquerda=%.1f cm  direita=%.1f cm\n", dEsq, dDir);

  // ----- DESVIAR -----
  estado = "DESVIANDO";
  if (dEsq < DIST_LIVRE && dDir < DIST_LIVRE) {
    if (DEBUG) Serial.println(">> Beco sem saída: ré + meia-volta");
    darDeOmbros();
    re(TEMPO_RE);
    girarDireita(2 * TEMPO_GIRO_90);
  } else if (dEsq > dDir) {
    if (DEBUG) Serial.println(">> Virando à esquerda");
    acenarEsq(2);           // seta para a esquerda
    girarEsquerda(TEMPO_GIRO_90);
  } else {
    if (DEBUG) Serial.println(">> Virando à direita");
    acenarDir(2);           // seta para a direita
    girarDireita(TEMPO_GIRO_90);
  }

  // recomeça a varredura e a detecção de "preso" do zero após desviar
  reiniciarVarredura();
  reiniciarPreso();
  ultimoBraco = millis();
}
