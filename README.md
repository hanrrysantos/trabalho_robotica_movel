# Robô Móvel Desviador de Obstáculos com Lógica Fuzzy

Robô de tração diferencial baseado em ESP32 que desvia de obstáculos usando lógica fuzzy. Um sensor ultrassônico montado sobre um servo varre a região à frente, e o controlador ajusta a velocidade conforme a distância: rápido com caminho livre, devagar perto de obstáculos e parado quando é necessário escanear outra direção.

O repositório mantém duas versões:

- `robo_com_braco`: versão principal e atualmente utilizada, com dois braços, controle pelo celular via Wi-Fi e modos automático e manual;
- `robo_sem_braco`: versão simplificada, sem braços e sem a interface Wi-Fi.

## Componentes

| Componente | Função |
|---|---|
| ESP32 DevKit (ESP-WROOM-32, USB-C, chip USB CP2102) | Controlador principal e ponto de acesso Wi-Fi |
| Driver de motor TB6612FNG | Controla os dois motores |
| 2 motores DC com redução (motor TT amarelo) + rodas de 65 mm | Tração diferencial |
| Rodízio giratório | Terceiro apoio, na frente |
| Sensor ultrassônico HC-SR04 | Mede a distância até obstáculos |
| 1 servo SG90 (cabeça) | Gira o sensor para varrer a frente e olhar para os lados |
| 2 servos SG90 (braços) | Movem os braços esquerdo e direito na versão principal |
| 3 resistores de 1 kΩ | Divisor de tensão do ECHO do sensor (5 V → 3,3 V) |
| Protoboard de 400 pontos e jumpers | Ligações |
| Power bank TOMATE TK2001 (5 V) | Alimentação |
| Chassi impresso em 3D | Estrutura |

## Estrutura do repositório

```text
trabalho_robotica_movel/
├── README.md
├── robo_sem_braco/
│   └── robo_sem_braco.ino     ← versão simplificada, sem braços
├── robo_com_braco/
│   └── robo_com_braco.ino     ← versão principal, com dois braços e Wi-Fi
├── teste_montagem/
│   └── teste_montagem.ino     ← testa e calibra cada componente
└── docs/
    ├── montagem.md            ← ligações e mapa de pinos
    ├── logica_fuzzy.md        ← controlador e modos de operação
    └── calibracao.md          ← testes e ajustes do código
```

Cada sketch `.ino` fica numa pasta com o mesmo nome porque o Arduino IDE exige essa correspondência.

## Como gravar o código

1. Em *Arquivo → Preferências* no Arduino IDE, adicione esta URL em “URLs adicionais para Gerenciadores de Placas”:

   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`

2. Em *Ferramentas → Placa → Gerenciador de Placas*, instale **esp32 by Espressif Systems**, versão **3.x**. O código usa a API LEDC dessa versão.
3. Não é preciso instalar bibliotecas adicionais. `WiFi.h` e `WebServer.h` fazem parte do pacote ESP32, e os servos são controlados diretamente pelo PWM.
4. Abra o sketch desejado, selecione *Ferramentas → Placa → ESP32 Dev Module* e escolha a porta da placa.
5. Clique em **Carregar (→)**. Se a gravação travar em “Connecting…”, mantenha o botão **BOOT** pressionado até ela começar.

A placa usa o conversor USB CP2102. Se o Windows não criar uma porta COM, instale o **CP210x Universal Windows Driver** da Silicon Labs. O Monitor Serial deve ser aberto a **115200 baud**.

> Grave e teste o robô com as rodas levantadas. Na versão principal, as rodas começam paradas, mas os dois braços acenam durante a inicialização.

## Alimentação

O power bank alimenta o robô por dois cabos:

- um cabo USB-C ligado diretamente ao ESP32;
- um cabo USB cortado ligado à porta USB-A do power bank, levando 5 V aos trilhos da protoboard: vermelho no trilho + e branco no trilho − neste cabo.

Os trilhos alimentam o driver dos motores, o sensor e os três servos. Todos os GNDs devem estar interligados. Consulte [docs/montagem.md](docs/montagem.md) antes de energizar o circuito.

## Controle pelo celular

A versão `robo_com_braco` cria sua própria rede Wi-Fi e não depende de roteador nem de internet:

1. Ligue o robô.
2. No celular, conecte-se à rede **wally 2.0**, usando a senha **12345678**.
3. Abra **http://192.168.4.1** no navegador.
4. Use **LIGAR/PAUSAR** e selecione o modo **Automático** ou **Manual**.

O robô inicia pausado. Mesmo parado, o painel continua mostrando a distância, os graus de pertinência e a velocidade que o controlador fuzzy calcularia.

No modo manual, segure uma seta para movimentar o robô. Ele para se deixar de receber comandos do celular por 600 ms. O avanço também é bloqueado quando o sensor detecta um obstáculo a menos de 15 cm; ré e giros ainda exigem supervisão.

## Comportamento automático

1. **Avançar e varrer:** a cabeça alterna entre centro, direita, centro e esquerda. Leituras laterais que estão fora da largura do robô são ignoradas, e a menor distância no caminho alimenta o controlador fuzzy.
2. **Controlar a velocidade:** o fuzzy converte a distância em uma velocidade de 0 a 100%, depois mapeada para o PWM dos motores.
3. **Detectar bloqueio:** se a distância frontal não variar enquanto as rodas deveriam estar movimentando o robô, ele considera que ficou preso, pede socorro com os braços e dá ré.
4. **Escanear:** quando a velocidade fuzzy cai abaixo de 20%, o robô freia e olha para a direita e para a esquerda. Se o obstáculo estiver a menos de 12 cm, dá ré antes.
5. **Desviar:** gira cerca de 90° para o lado com mais espaço. Se os dois lados estiverem bloqueados, dá ré e faz uma meia-volta.

Os dois braços acompanham o estado do robô:

| Situação | Movimento |
|---|---|
| Inicialização | Acenam alternadamente |
| Andando | Balançam em oposição, com ritmo e amplitude proporcionais à velocidade fuzzy |
| Obstáculo muito perto | Sobem juntos, simulando susto |
| Robô preso | Agitam no alto, pedindo socorro |
| Olhando para um lado | O braço daquele lado aponta a direção |
| Antes de virar | O braço do lado da curva acena |
| Beco sem saída | Os dois dão de ombros |

A teoria e os demais modos estão detalhados em [docs/logica_fuzzy.md](docs/logica_fuzzy.md).

## Estado atual

A versão-base atual é `robo_com_braco/robo_com_braco.ino`, com:

- dois braços;
- varredura frontal durante o movimento;
- detecção de robô preso;
- controle automático e manual pelo celular;
- painel de telemetria fuzzy;
- início pausado por segurança.

O sketch `robo_sem_braco` continua disponível como referência simplificada e não possui todos esses recursos.

## Cuidados e ajustes pendentes

- Refaça os testes de montagem e a calibração sempre que um servo ou braço for reposicionado.
- Confirme no chão os tempos de giro, a correção entre as rodas e os limites dos dois braços.
- Se o ESP32 reiniciar ao acionar motores ou servos, verifique a alimentação e instale um capacitor eletrolítico de 470 a 1000 µF entre os trilhos + e −, respeitando a polaridade.
- O HC-SR04 pode detectar mal superfícies inclinadas ou macias, como tecido e almofadas.
- A rede e a senha estão gravadas no sketch. Altere `WIFI_NOME` e `WIFI_SENHA` antes de usar o robô fora de uma demonstração controlada.
