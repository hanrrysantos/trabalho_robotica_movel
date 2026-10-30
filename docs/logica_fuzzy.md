# Lógica fuzzy e controle

## Visão geral

O robô funciona como uma **máquina de estados** com três estados. O **controlador fuzzy** é usado no estado de avanço, para transformar a distância medida pelo sensor em velocidade.

```
            ┌──────────────────────────────────────────┐
            ▼                                          │
       ┌─────────┐  velocidade fuzzy < 20%  ┌──────────┐│
       │ AVANÇAR │ ───────────────────────► │ ESCANEAR ││
       └─────────┘                          └──────────┘│
                                                 │      │
                                                 ▼      │
                                            ┌─────────┐ │
                                            │ DESVIAR │─┘
                                            └─────────┘
```

- **AVANÇAR:** lê a distância à frente (mediana de 3 leituras, para descartar leituras falsas), calcula a velocidade pelo controlador fuzzy e aciona os motores.
- **ESCANEAR:** freia, dá ré se o obstáculo estiver a menos de 12 cm, e gira o sensor para a direita (20°) e para a esquerda (160°) para medir os dois lados.
- **DESVIAR:** gira cerca de 90° para o lado com mais espaço. Se os dois lados tiverem menos de 30 cm livres, dá ré e gira cerca de 180°.

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

## Saída de depuração

Com `DEBUG = true`, cada leitura aparece no Monitor Serial (115200 baud):

```
d=  40.0 cm | MP=0.00 P=0.33 M=0.20 L=0.00 | vel= 41.6%
>> Obstáculo! esquerda=85.3 cm  direita=22.1 cm
>> Virando à esquerda
```

Esses dados podem ser copiados para uma planilha para gerar gráficos para o relatório.

## Como alterar o controlador

No código, os conjuntos ficam nas tabelas `entradaDist` e `saidaVel`, e as regras ficam no vetor `regras` (a entrada `i` aponta para a saída `regras[i]`).

- **Para o robô frear mais cedo:** desloque os conjuntos de entrada para distâncias maiores (por exemplo, MUITO_PERTO até 20–30 cm).
- **Para mudar a velocidade em cada faixa:** mova os picos dos conjuntos de saída.
- **Para acrescentar um conjunto:** adicione uma linha em cada tabela, uma posição no vetor `regras` e ajuste `N_REGRAS`. Se a saída de depuração for usada, inclua o novo grau no `Serial.printf`.
