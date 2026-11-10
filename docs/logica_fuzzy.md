# Lógica fuzzy e controle

## Visão geral

A versão principal inicia em **PAUSADO** e pode operar em dois modos escolhidos pelo celular:

- **Automático:** usa a varredura frontal, a lógica fuzzy e a máquina de estados de desvio.
- **Manual:** responde às setas da página; o fuzzy continua sendo calculado apenas para telemetria.

No automático, o fluxo principal é:

```
       ┌────────────────────────────────────────────────────┐
       ▼                                                    │
  ┌──────────┐  velocidade < 20% ou preso  ┌──────────┐    │
  │ AVANÇAR  │ ──────────────────────────► │ ESCANEAR │    │
  │ + VARRER │                              └────┬─────┘    │
  └──────────┘                                   ▼          │
                                            ┌─────────┐     │
                                            │ DESVIAR │ ────┘
                                            └─────────┘
```

- **AVANÇAR + VARRER:** a cabeça alterna entre centro, direita, centro e esquerda. Cada posição usa a mediana de três leituras.
- **FILTRAR O CAMINHO:** uma leitura inclinada só entra no controle se sua projeção lateral estiver dentro da largura do robô. A menor distância válida alimenta o fuzzy.
- **DETECTAR PRESO:** se a distância central não mudar o suficiente durante 2,5 s enquanto o robô deveria avançar, ele para, sinaliza com os braços e dá ré.
- **ESCANEAR:** freia, dá ré se o obstáculo estiver a menos de 12 cm e mede os lados em 20° e 160°.
- **DESVIAR:** gira cerca de 90° para o lado mais livre. Se ambos tiverem menos de 30 cm, dá ré e gira aproximadamente 180°.

Uma troca de modo, um comando de pausa ou a perda de comandos manuais interrompe o movimento. No modo manual, o avanço é bloqueado abaixo de 15 cm, mas ré e giros continuam sob responsabilidade do operador.

## O controlador fuzzy

É um controlador do tipo **Mamdani**, escrito à mão no código, sem biblioteca. Tem uma entrada e uma saída.

### Entrada: distância (cm)

| Conjunto | Forma | Pontos (cm) | Significado |
|---|---|---|---|
| MUITO_PERTO | Trapézio (ombro esquerdo) | 0, 0, 15, 25 | Totalmente "muito perto" até 15 cm |
| PERTO | Triângulo | 15, 30, 45 | Pico em 30 cm |
| MEDIO | Triângulo | 35, 60, 90 | Pico em 60 cm |
| LONGE | Trapézio (ombro direito) | 75, 120, 500, 500 | Totalmente "longe" a partir de 120 cm |

### Saída: velocidade (% da velocidade máxima)

| Conjunto | Forma | Pontos (%) |
|---|---|---|
| PARAR | Triângulo (ombro esquerdo) | 0, 0, 15 |
| LENTA | Triângulo | 10, 30, 50 |
| MEDIA | Triângulo | 40, 60, 80 |
| RAPIDA | Trapézio (ombro direito) | 70, 90, 100, 100 |

### Base de regras

1. **SE** distância é MUITO_PERTO **ENTÃO** velocidade é PARAR
2. **SE** distância é PERTO **ENTÃO** velocidade é LENTA
3. **SE** distância é MEDIO **ENTÃO** velocidade é MEDIA
4. **SE** distância é LONGE **ENTÃO** velocidade é RAPIDA

### Etapas do cálculo

1. **Fuzzificação:** calcula o grau de pertinência (0 a 1) da distância medida em cada conjunto de entrada. Uma distância de 40 cm, por exemplo, é 0,33 PERTO e 0,2 MEDIO ao mesmo tempo.
2. **Implicação (mínimo):** cada regra "corta" o seu conjunto de saída na altura do grau de pertinência da entrada correspondente.
3. **Agregação (máximo):** junta os conjuntos de saída cortados numa única figura.
4. **Defuzzificação (centroide):** calcula o centro de massa dessa figura, percorrendo a saída de 0 a 100% em passos de 1%. O resultado é a velocidade em porcentagem.

A função de pertinência é a mesma para todos os conjuntos: um trapézio definido por 4 pontos (a, b, c, d). Quando b = c, ele vira triângulo; quando a = b ou c = d, vira ombro.

### Da velocidade fuzzy para os motores

- Se a velocidade fuzzy for **menor que 20%** (`VEL_LIMIAR`), o robô considera que há um obstáculo e passa para o estado ESCANEAR.
- Caso contrário, a velocidade é convertida linearmente para o PWM dos motores: 20% vira `PWM_MIN` (110) e 100% viraria `PWM_MAX` (200).
- Antes de chegar às rodas, o comando é multiplicado por `FATOR_ESQ` e `FATOR_DIR` para compensar diferenças mecânicas. Na configuração atual, os fatores são 0,85 e 1,00.

### Curva distância × velocidade

Valores calculados com os parâmetros atuais do código:

| Distância (cm) | Graus (MP / P / M / L) | Velocidade fuzzy | PWM | Ação |
|---|---|---|---|---|
| 5 a 15 | 1 / 0 / 0 / 0 | 4,7% | – | Para e escaneia |
| 20 | 0,5 / 0,33 / 0 / 0 | 21,7% | 112 | Muito devagar |
| 25 a 35 | 0 / 0,67–1 / 0 / 0 | 30,0% | 121 | Lenta |
| 40 | 0 / 0,33 / 0,2 / 0 | 41,6% | 134 | Lenta → média |
| 50 a 70 | 0 / 0 / 0,6–1 / 0 | 60,0% | 155 | Média |
| 80 | 0 / 0 / 0,33 / 0,11 | 65,3% | 161 | Média → rápida |
| 90 | 0 / 0 / 0 / 0,33 | 86,9% | 185 | Rápida |
| 120 ou mais | 0 / 0 / 0 / 1 | 89,4% | 188 | Rápida |

Dois detalhes que aparecem na tabela:

- Nas faixas em que só uma regra está ativa (25 a 35 cm, 50 a 70 cm), a velocidade fica constante, porque o centroide de um único triângulo cortado continua no centro dele. A velocidade só muda nas transições entre conjuntos.
- A velocidade máxima fica em cerca de 89%, e não 100%, porque o centroide do conjunto RAPIDA fica antes do fim do intervalo. Por isso o PWM máximo real é cerca de 188.

## Telemetria e saída de depuração

Com `DEBUG = true`, cada ciclo automático aparece no Monitor Serial a 115200 baud. `D`, `C` e `E` são as leituras da varredura à direita, centro e esquerda; `d` é a distância efetivamente usada pelo fuzzy:

```
D= 55.0 C= 40.0 E= 70.0 -> d= 40.0 cm | MP=0.00 P=0.33 M=0.20 L=0.00 | vel= 41.6%
>> Obstáculo! esquerda=85.3 cm  direita=22.1 cm
>> Virando à esquerda
```

O painel em `http://192.168.4.1` mostra as três distâncias, os quatro graus de pertinência e a velocidade fuzzy mesmo quando o robô está pausado. Esses dados também podem ser registrados para gerar gráficos do relatório.

## Como alterar o controlador

No código, os conjuntos ficam nas tabelas `entradaDist` e `saidaVel`, e as regras ficam no vetor `regras` (a entrada `i` aponta para a saída `regras[i]`).

- **Para o robô frear mais cedo:** desloque os conjuntos de entrada para distâncias maiores (por exemplo, MUITO_PERTO até 20–30 cm).
- **Para mudar a velocidade em cada faixa:** mova os picos dos conjuntos de saída.
- **Para acrescentar um conjunto:** adicione uma linha em cada tabela, uma posição no vetor `regras` e ajuste `N_REGRAS`. Se a saída de depuração for usada, inclua o novo grau no `Serial.printf`.
