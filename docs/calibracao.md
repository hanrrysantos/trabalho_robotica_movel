# Testes e calibração

## Programa de teste (`teste_montagem`)

Serve para conferir cada peça separadamente. É o primeiro programa a rodar depois de qualquer mudança na montagem.

1. Levante o robô, com as rodas no ar.
2. Grave o `teste_montagem.ino` e abra o Monitor Serial a **115200 baud**, com fim de linha em "New Line".
3. Digite o número do teste e Enter, e compare o resultado com o "ESPERADO" que aparece na tela.

| Opção | Teste | Resultado esperado |
|---|---|---|
| 1 | Roda esquerda | Gira para frente por 2 s e depois para trás |
| 2 | Roda direita | Gira para frente por 2 s e depois para trás |
| 3 | As duas rodas | Giram para frente juntas |
| 4 | Servo da cabeça | Olha para a frente, para a direita do robô, para a esquerda e volta |
| 5 | Sensor | 20 leituras de distância que mudam ao aproximar e afastar a mão |
| 6 | Braço | Modo de calibração do braço (veja abaixo) |

Para os motores girarem nos testes, o cabo cortado do power bank precisa estar ligado na protoboard.

## Ajustes do programa principal

Todos ficam na seção `AJUSTES (calibrar no robô)`, no topo do `robo_sem_braco.ino` e do `robo_com_braco.ino`.

| Constante | Valor atual | O que faz | Quando mudar |
|---|---|---|---|
| `INVERTE_ESQ` / `INVERTE_DIR` | false | Inverte o sentido de uma roda | Se uma roda girar ao contrário |
| `PWM_MIN` | 110 | Menor PWM usado para andar | Se o robô não sai do lugar ou "engasga", aumente (130–150) |
| `PWM_MAX` | 200 | Maior PWM usado para andar | Diminua se o robô estiver rápido demais e encostando nos obstáculos |
| `PWM_GIRO` | 170 | PWM usado nos giros e na ré | Se o giro estiver fraco ou violento |
| `SERVO_CENTRO` | 90 | Ângulo da cabeça olhando para a frente | Se o sensor não ficar reto |
| `SERVO_DIREITA` / `SERVO_ESQUERDA` | 20 / 160 | Ângulos para olhar para cada lado | Se os lados saírem trocados, troque os dois valores |
| `VEL_LIMIAR` | 20.0 | Velocidade fuzzy (%) abaixo da qual o robô considera que há obstáculo | Aumente para ele parar mais cedo |
| `DIST_LIVRE` | 30.0 | Distância (cm) mínima para considerar um lado livre | Se ele escolher lados apertados demais |
| `DIST_RE` | 12.0 | Distância (cm) abaixo da qual ele dá ré antes de escanear | Se ele bater ao girar |
| `TEMPO_GIRO_90` | 350 | Tempo (ms) para girar cerca de 90° | Cronometre: se girar demais, diminua; se girar de menos, aumente |
| `TEMPO_RE` | 350 | Tempo (ms) de ré | Se a ré for curta ou longa demais |
| `DEBUG` | true | Mostra as leituras no Monitor Serial | Pode deixar sempre ligado |

Os valores atuais (`PWM_MAX = 200` e `VEL_LIMIAR = 20`) foram encontrados nos testes no chão: com eles, o robô para antes de encostar nos obstáculos.

## Calibração do braço (só na versão com braço)

O ângulo do servo depende de como o braço foi encaixado no eixo. Se o braço for desmontado e montado de novo, refaça a calibração:

1. Grave o `teste_montagem.ino`, abra o Monitor Serial e digite **6**. O servo do braço vai para 90°.
2. Solte o parafuso do braço, encaixe o braço **esticado para a frente** (na horizontal) e aperte de novo.
3. Digite **120** e Enter, e observe o braço:
   - se ele **subiu**, use `BRACO_CIMA = 180`;
   - se ele **desceu**, use `BRACO_CIMA = 0` (valor atual deste robô).
4. Teste o valor escolhido para confirmar que o braço aponta para cima. Mude o ângulo de 10 em 10 graus para o braço não bater em nada.
5. Digite **-1** para sair.

No `robo_com_braco.ino`, as constantes ficam assim:

```cpp
const int BRACO_FRENTE = 90;   // braço esticado para a frente
const int BRACO_CIMA   = 0;    // braço apontando para cima
```

O código move o braço somente entre essas duas posições.
