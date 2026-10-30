# Robô Móvel Desviador de Obstáculos com Lógica Fuzzy

Robô de tração diferencial que anda sozinho e desvia de obstáculos. Um sensor ultrassônico mede a distância à frente e um **controlador fuzzy** decide a velocidade: o robô anda rápido quando o caminho está livre, desacelera suavemente ao se aproximar de algo e, quando chega perto demais, para, olha para os lados e gira para onde há mais espaço.

Existem duas versões do programa: uma **sem braço** e outra **com um braço** no lado esquerdo, que acena, balança no ritmo da velocidade e "se assusta" com obstáculos.

## Componentes

| Componente | Função |
|---|---|
| ESP32 DevKit (ESP-WROOM-32, USB-C, chip USB CP2102) | Controlador principal |
| Driver de motor TB6612FNG | Controla os dois motores |
| 2 motores DC com redução (motor TT amarelo) + rodas de 65 mm | Tração diferencial |
| Rodízio giratório | Terceiro apoio, na frente |
| Sensor ultrassônico HC-SR04 | Mede a distância até obstáculos |
| Servo SG90 (cabeça) | Gira o sensor para olhar para os lados |
| Servo SG90 (braço) | Braço esquerdo, só na versão com braço |
| 3 resistores de 1 kΩ | Divisor de tensão do ECHO do sensor (5 V → 3,3 V) |
| Protoboard de 400 pontos e jumpers | Ligações |
| Power bank TOMATE TK2001 (5 V) | Alimentação |
| Chassi impresso em 3D | Estrutura |

## Estrutura do repositório

```
trabalho_robotica_movel/
├── README.md
├── robo_sem_braco/
│   └── robo_sem_braco.ino     ← programa do robô sem o braço
├── robo_com_braco/
│   └── robo_com_braco.ino     ← mesmo programa, com o braço esquerdo
├── teste_montagem/
│   └── teste_montagem.ino     ← testa cada peça separadamente pelo Monitor Serial
└── docs/
    ├── montagem.md            ← ligações de todos os componentes
    ├── logica_fuzzy.md        ← como o controlador fuzzy funciona
    └── calibracao.md          ← o que cada ajuste do código faz
```

Cada `.ino` fica numa pasta com o mesmo nome porque o Arduino IDE exige isso.

## Como gravar o código

1. **Instalar a placa ESP32 no Arduino IDE:** em *Arquivo → Preferências*, adicionar em "URLs adicionais para Gerenciadores de Placas":
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   Depois, em *Ferramentas → Placa → Gerenciador de Placas*, instalar **esp32 by Espressif Systems**, **versão 3.x**. O código usa funções que não existem na versão 2.x.
2. **Nenhuma biblioteca extra é necessária.** Os servos são controlados direto pelo PWM do ESP32.
3. **Driver USB:** a placa usa o chip **CP2102**. Se o Windows não criar uma porta COM ao conectar a placa, instalar o driver **CP210x Universal Windows Driver** da Silicon Labs.
4. Abrir o `.ino`, escolher *Ferramentas → Placa → **ESP32 Dev Module*** e a porta da placa (no Gerenciador de Dispositivos aparece como "Silicon Labs CP210x"; a COM1 normalmente **não** é a placa).
5. Clicar em **Carregar (→)**. Se travar em "Connecting…", segurar o botão **BOOT** da placa até a gravação começar.

O Monitor Serial funciona a **115200 baud**.

> ⚠️ Assim que a gravação termina, o programa começa a rodar e as rodas giram depois de cerca de 2 segundos. Grave com o robô levantado.

## Alimentação

O power bank alimenta o robô por dois cabos:

- um **cabo USB-C** ligado direto no ESP32;
- um **cabo USB cortado**, ligado na porta USB-A do power bank, que leva 5 V para os trilhos da protoboard: **vermelho no trilho +** e **branco no trilho −** (nesse cabo, o branco é o GND).

Os trilhos da protoboard alimentam o driver dos motores (VM), o sensor e os servos. Detalhes em [docs/montagem.md](docs/montagem.md).

## Comportamento

1. **Avançar:** o controlador fuzzy transforma a distância medida em velocidade.
2. **Escanear:** quando a velocidade fuzzy cai abaixo de 20%, o robô freia e o servo da cabeça olha para a direita e para a esquerda. Se estiver perto demais (menos de 12 cm), dá ré antes.
3. **Desviar:** gira cerca de 90° para o lado com mais espaço. Se os dois lados estiverem bloqueados, dá ré e meia-volta.

Na versão com braço, o braço também reage:

| Situação | Braço |
|---|---|
| Ao ligar | Acena "tchau" por cerca de 2 s |
| Andando | Balança, mais rápido e mais alto quanto maior a velocidade fuzzy |
| Muito perto do obstáculo | Levanta de uma vez (susto) e o robô dá ré |
| Vai virar à esquerda | Acena 2 vezes antes de girar |
| Beco sem saída | "Dá de ombros" antes da meia-volta |

A teoria está em [docs/logica_fuzzy.md](docs/logica_fuzzy.md).

## Estado atual e próximos passos

**Funcionando:**

- Montagem conferida ligação por ligação, e todos os testes do `teste_montagem` passaram.
- Desvio de obstáculos com lógica fuzzy calibrado no chão: para bem e desvia bem, inclusive quando um obstáculo aparece de repente na frente.
- Braço esquerdo instalado e calibrado (`BRACO_CIMA = 0`).

**Pendências conhecidas:**

- Uma das rodas está um pouco solta no eixo, e o robô puxa levemente para a direita. Um pouco de cola quente ou um parafuso no eixo deve resolver.
- O robô ainda não tem capacitor na alimentação. Se o ESP32 começar a reiniciar sozinho (para do nada e volta 2 segundos depois), colocar um capacitor eletrolítico de 470 a 1000 µF entre os trilhos + e − da protoboard, com a perna marcada pela faixa no −.
- O sensor ultrassônico tem dificuldade com superfícies inclinadas e objetos macios (tecido, almofada), que só são detectados bem de perto. É uma limitação do HC-SR04.

**Ideias para continuar:**

- **Segundo braço** (lado direito) no pino **D17** (marcado como TX2 na placa), usando o canal de PWM 6.
- **Fuzzy com as distâncias laterais**, para o robô desviar em curva suave sem precisar parar.
- **Liga/desliga pelo botão BOOT** da placa, para não precisar mexer nos cabos para parar o robô.
- **Gráficos do controlador fuzzy** para o relatório (funções de pertinência e curva distância × velocidade).
