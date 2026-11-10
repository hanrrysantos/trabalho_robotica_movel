# Montagem e ligações

## Convenção de direita e esquerda

**Direita e esquerda são sempre do ponto de vista do robô:** imagine-se sentado nele, olhando para onde o sensor aponta. Ao olhar o robô de frente, os lados ficam invertidos.

## Mapa de pinos do ESP32

A tabela corresponde à versão principal, `robo_com_braco.ino`.

| GPIO | Identificação comum na placa | Ligado em |
|---|---|---|
| 4 | D4 | TB6612 – STBY |
| 5 | D5 | HC-SR04 – TRIG |
| 13 | D13 | Servo da cabeça – sinal |
| 16 | RX2 | Servo do braço direito – sinal |
| 17 | TX2 | Servo do braço esquerdo – sinal |
| 18 | D18 | TB6612 – BIN1 |
| 19 | D19 | TB6612 – BIN2 |
| 23 | D23 | TB6612 – PWMB |
| 25 | D25 | TB6612 – AIN1 |
| 26 | D26 | TB6612 – AIN2 |
| 27 | D27 | TB6612 – PWMA |
| 33 | D33 | HC-SR04 – ECHO, depois do divisor de tensão |
| 3V3 | 3V3 | TB6612 – VCC |
| GND | GND | Trilho − da protoboard |

Algumas placas imprimem apenas o número, enquanto outras usam os nomes RX2 e TX2. Confirme sempre o **GPIO** antes de ligar. A versão `robo_sem_braco` não utiliza os GPIOs 16 e 17.

Evite usar GPIO 6 a 11, ligados à memória flash, e GPIO 12, que pode interferir na inicialização. GPIO 34, 35, 36 e 39 funcionam apenas como entrada.

## Alimentação

| De | Para |
|---|---|
| Power bank – cabo USB-C | Conector USB-C do ESP32 |
| Power bank – cabo USB cortado, fio vermelho | Trilho + da protoboard (5 V) |
| Power bank – cabo USB cortado, fio branco neste cabo | Trilho − da protoboard (GND) |
| GND do ESP32 | Trilho − |

O GND do ESP32 precisa estar ligado ao trilho − para que a placa, o driver, o sensor e os servos compartilhem a mesma referência. Sem GND comum, os sinais de controle não funcionam corretamente.

O cabo cortado deve ser conferido com um multímetro antes da ligação, pois as cores podem variar. O circuito deve receber **5 V**, nunca 9 V ou 12 V.

Motores e três servos podem causar picos de corrente. Se o ESP32 reiniciar durante movimentos, verifique os cabos e a capacidade da fonte. Um capacitor eletrolítico de 470 a 1000 µF entre os trilhos + e − pode ajudar; ligue o terminal marcado com a faixa no −.

## Driver TB6612FNG

| Pino do TB6612 | Ligado em |
|---|---|
| VM | Trilho + (5 V, alimentação dos motores) |
| VCC | 3V3 do ESP32 (lógica do driver) |
| GND | Trilho − |
| STBY | GPIO 4 |
| PWMA / AIN1 / AIN2 | GPIO 27 / 25 / 26 |
| PWMB / BIN1 / BIN2 | GPIO 23 / 18 / 19 |
| AO1 / AO2 | Motor direito: AO1 no fio vermelho, AO2 no preto |
| BO1 / BO2 | Motor esquerdo: BO1 no fio preto, BO2 no vermelho |

O canal A controla a roda direita e o canal B controla a esquerda. Se uma roda girar ao contrário, altere `INVERTE_ESQ` ou `INVERTE_DIR` para `true`, sem trocar a lógica do restante do programa.

## Sensor HC-SR04

| Pino do sensor | Ligado em |
|---|---|
| VCC | Trilho + (5 V) |
| GND | Trilho − |
| TRIG | GPIO 5, diretamente |
| ECHO | Divisor de tensão → GPIO 33 |

O ECHO do HC-SR04 trabalha em 5 V, mas o ESP32 aceita aproximadamente 3,3 V. O sinal deve passar pelo divisor com três resistores de 1 kΩ:

```text
ECHO ── 1 kΩ ──┬── 1 kΩ ── 1 kΩ ── GND
               │
             GPIO 33
```

Há 1 kΩ entre ECHO e GPIO 33 e 2 kΩ entre GPIO 33 e GND. A saída fica em aproximadamente 5 V × 2/3 = 3,3 V.

Posição registrada na protoboard:

| Peça | Furos |
|---|---|
| Fio do ECHO | j13 |
| Resistor 1 (ECHO → GPIO 33) | h13 – h9 |
| Fio para GPIO 33 | f9 |
| Resistor 2 | i9 – i16 |
| Resistor 3 | h16 – h20 |
| Fio para o trilho − | j20 |

## Servos SG90

| Fio do servo | Cabeça | Braço esquerdo | Braço direito |
|---|---|---|---|
| Marrom | Trilho − | Trilho − | Trilho − |
| Vermelho | Trilho + (5 V) | Trilho + (5 V) | Trilho + (5 V) |
| Laranja (sinal) | GPIO 13 | GPIO 17 (TX2) | GPIO 16 (RX2) |

Nunca ligue o fio vermelho de um servo ao pino 3V3.

Os braços são instalados nos lados correspondentes do chassi. O programa principal limita cada um ao intervalo entre “esticado para a frente” e “apontando para cima”, evitando o chão e as rodas. Como os servos ficam espelhados, os ângulos de “cima” normalmente são opostos: na configuração atual, 0° para o esquerdo e 180° para o direito.

Antes de fixar definitivamente as hastes, execute as opções 6 e 7 de `teste_montagem.ino` e siga [calibracao.md](calibracao.md).

## Lembretes sobre a protoboard

- Os cinco furos de uma linha (a–e ou f–j) são interligados.
- O canal central separa os lados: o furo “e” não está ligado ao “f”.
- Os trilhos + e − podem ser interrompidos no meio em algumas protoboards; confira a continuidade.
- Desligue a alimentação antes de mover jumpers, servos ou conexões do driver.
