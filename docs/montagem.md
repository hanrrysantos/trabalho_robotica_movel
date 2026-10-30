# Montagem e ligações

## Convenção de direita e esquerda

**Direita e esquerda são sempre do ponto de vista do robô:** imagine-se sentado nele, olhando para onde o sensor aponta. Olhando o robô de frente, os lados ficam invertidos.

## Mapa de pinos do ESP32

| GPIO | Nome na placa | Ligado em |
|---|---|---|
| D4 | D4 | TB6612 – STBY |
| D5 | D5 | HC-SR04 – TRIG |
| D13 | D13 | Servo da cabeça – sinal (laranja) |
| D16 | **RX2** | Servo do braço – sinal (laranja), só na versão com braço |
| D18 | D18 | TB6612 – BIN1 |
| D19 | D19 | TB6612 – BIN2 |
| D23 | D23 | TB6612 – PWMB |
| D25 | D25 | TB6612 – AIN1 |
| D26 | D26 | TB6612 – AIN2 |
| D27 | D27 | TB6612 – PWMA |
| D33 | D33 | HC-SR04 – ECHO (pelo divisor de tensão) |
| 3V3 | 3V3 | TB6612 – VCC |
| GND | GND | Trilho − da protoboard |

Pinos que **não** devem ser usados em expansões: GPIO 6 a 11 (ligados à memória flash), GPIO 12 (pode impedir a placa de ligar) e GPIO 34, 35, 36 e 39 como saída (só funcionam como entrada).

## Alimentação

| De | Para |
|---|---|
| Power bank – cabo USB-C | Conector USB-C do ESP32 |
| Power bank – cabo USB cortado, fio **vermelho** | Trilho **+** da protoboard (5 V) |
| Power bank – cabo USB cortado, fio **branco** | Trilho **−** da protoboard (GND) |
| GND do ESP32 | Trilho **−** |

O GND do ESP32 precisa estar no trilho − para que o ESP32 e o driver tenham a mesma referência de 0 V. Sem isso, os sinais de controle não funcionam.

O power bank aceita carga rápida (9 V e 12 V), mas, pelo cabo cortado e sem os fios de dados, ele fornece apenas 5 V. Antes de ligar um cabo cortado novo no circuito, confirme com o multímetro que ele fornece 5 V.

## Driver TB6612FNG

| Pino do TB6612 | Ligado em |
|---|---|
| VM | Trilho + (5 V, alimenta os motores) |
| VCC | 3V3 do ESP32 (alimenta a lógica do driver) |
| GND | Trilho − (os GNDs do módulo são ligados por dentro; um basta) |
| STBY | D4 |
| PWMA / AIN1 / AIN2 | D27 / D25 / D26 |
| PWMB / BIN1 / BIN2 | D23 / D18 / D19 |
| AO1 / AO2 | Motor **direito**: AO1 no fio vermelho, AO2 no preto |
| BO1 / BO2 | Motor **esquerdo**: BO1 no fio preto, BO2 no vermelho |

O **canal A controla a roda direita** e o **canal B controla a roda esquerda**. O código já considera isso.

A ordem dos dois fios de um motor dentro do canal só muda o sentido de giro da roda. Se uma roda girar ao contrário, basta mudar `INVERTE_ESQ` ou `INVERTE_DIR` para `true` no código, sem mexer nos fios.

## Sensor HC-SR04

| Pino do sensor | Ligado em |
|---|---|
| VCC | Trilho + (5 V) |
| GND | Trilho − |
| TRIG | D5 (direto) |
| ECHO | Divisor de tensão → D33 |

O sensor trabalha com 5 V, mas o ESP32 só aceita 3,3 V nos pinos. Por isso o ECHO passa por um divisor de tensão feito com três resistores de 1 kΩ:

```
ECHO ── 1 kΩ ──┬── 1 kΩ ── 1 kΩ ── GND
               │
              D33
```

Em cima fica 1 kΩ e embaixo ficam 2 kΩ (dois resistores em série). A tensão no D33 fica em 5 V × 2k / 3k ≈ **3,3 V**.

Posição atual na protoboard:

| Peça | Furos |
|---|---|
| Fio do ECHO | j13 |
| Resistor 1 (ECHO → D33) | h13 – h9 |
| Fio para o D33 | f9 |
| Resistor 2 | i9 – i16 |
| Resistor 3 | h16 – h20 |
| Fio para o trilho − | j20 |

## Servos SG90

| Fio do servo | Servo da cabeça | Servo do braço |
|---|---|---|
| Marrom | Trilho − | Trilho − |
| Vermelho | Trilho + (5 V) | Trilho + (5 V) |
| Laranja (sinal) | D13 | D16 (RX2) |

Os servos precisam dos 5 V do trilho +. Nunca ligue o vermelho no 3V3.

O braço é preso no lado **esquerdo** do chassi e se move apenas entre "esticado para a frente" e "apontando para cima". O código não deixa o braço apontar para baixo nem para trás, para ele não bater no chão nem na roda.

## Lembretes sobre a protoboard

- Os 5 furos de uma linha (a–e, ou f–j) são ligados entre si, então qualquer furo da mesma linha serve.
- O canal do meio separa os lados: o furo e de uma linha **não** está ligado ao furo f.
- Os trilhos + e − correm ao longo da placa. Em algumas protoboards o trilho é interrompido no meio, e as duas metades não se conectam.
