# Testes e calibração

## Programa de teste (`teste_montagem`)

Use `teste_montagem/teste_montagem.ino` para conferir cada peça separadamente depois de qualquer mudança física.

1. Levante o robô, deixando as rodas no ar.
2. Grave o sketch e abra o Monitor Serial a **115200 baud**, com fim de linha em “New Line”.
3. Digite a opção desejada e pressione Enter.
4. Compare o movimento com o resultado esperado mostrado no Monitor Serial.

| Opção | Teste | Resultado esperado |
|---|---|---|
| 1 | Roda esquerda | Gira para frente por 2 s e depois para trás |
| 2 | Roda direita | Gira para frente por 2 s e depois para trás |
| 3 | As duas rodas | Giram para frente juntas |
| 4 | Servo da cabeça | Centro, direita, esquerda e centro |
| 5 | Sensor | 20 leituras que mudam ao aproximar e afastar a mão |
| 6 | Braço esquerdo | Abre o modo de calibração do servo no GPIO 17 |
| 7 | Braço direito | Abre o modo de calibração do servo no GPIO 16 |

Para testar motores e servos, a alimentação de 5 V da protoboard deve estar conectada e todos os GNDs devem ser comuns.

## Ajustes de movimento

Os principais valores ficam na seção `AJUSTES (calibrar no robô)` dos sketches.

| Constante | Valor na versão principal | O que faz | Quando mudar |
|---|---:|---|---|
| `INVERTE_ESQ` / `INVERTE_DIR` | `false` / `false` | Inverte o sentido elétrico de cada roda | Se uma roda girar ao contrário |
| `FATOR_ESQ` / `FATOR_DIR` | `0.85` / `1.00` | Compensa diferenças entre os motores | Se o robô desviar ao tentar seguir reto |
| `PWM_MIN` | `110` | Menor PWM automático | Se o robô não sair do lugar ou engasgar |
| `PWM_MAX` | `200` | Maior PWM automático | Se estiver rápido demais |
| `PWM_GIRO` | `170` | PWM dos giros e da ré automática | Se o giro estiver fraco ou violento |
| `SERVO_CENTRO` | `90` | Cabeça apontada para a frente | Se o sensor não ficar alinhado |
| `SERVO_DIREITA` / `SERVO_ESQUERDA` | `20` / `160` | Posições do escaneamento lateral | Se a cabeça não alcançar os lados corretos |
| `VEL_LIMIAR` | `20.0` | Abaixo desta velocidade fuzzy, inicia o desvio | Aumente para parar mais cedo |
| `DIST_LIVRE` | `30.0` cm | Espaço mínimo para um lado ser considerado livre | Se escolher passagens estreitas |
| `DIST_RE` | `12.0` cm | Distância para dar ré antes de escanear | Se encostar ao iniciar o giro |
| `TEMPO_GIRO_90` | `350` ms | Duração aproximada de um giro de 90° | Ajuste medindo no piso usado |
| `TEMPO_RE` | `350` ms | Duração da ré automática | Se a ré for curta ou longa |
| `DEBUG` | `true` | Envia telemetria ao Monitor Serial | Desative somente se necessário |

### Calibração para andar reto

1. Com as rodas levantadas, confirme primeiro que ambas giram para frente.
2. Coloque o robô no chão, aponte-o para uma linha reta e mande avançar.
3. Ajuste apenas um fator por vez, em passos pequenos, por exemplo 0,02.
4. Se ele puxar para a direita, reduza `FATOR_ESQ`; se puxar para a esquerda, aumente-o ou reduza `FATOR_DIR`.
5. Mantenha os fatores entre 0 e 1 para não ultrapassar o PWM solicitado.

Repita o teste com bateria carregada e no mesmo tipo de piso da demonstração.

## Calibração da cabeça e da varredura

| Constante | Valor atual | Função |
|---|---:|---|
| `VARREDURA_ABERTURA` | `40`° | Deslocamento da cabeça para cada lado enquanto avança |
| `TEMPO_PASSO_SERVO` | `100` ms | Espera antes de medir cada posição da varredura |
| `MEIA_LARGURA` | `15.0` cm | Metade da largura do robô mais uma margem lateral |

- Alinhe fisicamente a cabeça e corrija `SERVO_CENTRO` antes de alterar os demais ângulos.
- Se o sensor ainda estiver se movendo no momento da leitura, aumente `TEMPO_PASSO_SERVO`.
- Meça metade da largura máxima do robô, incluindo rodas e braços quando recolhidos, e adicione uma pequena folga para definir `MEIA_LARGURA`.
- Uma abertura maior cobre melhor as quinas, mas aumenta o deslocamento do servo e pode deixar cada direção mais tempo sem atualização.

## Detecção de robô preso

| Constante | Valor atual | Função |
|---|---:|---|
| `TEMPO_PRESO` | `2500` ms | Tempo sem mudança suficiente na distância antes de declarar bloqueio |
| `VARIACAO_PRESO` | `4.0` cm | Variação mínima considerada movimento |

Se o robô pedir socorro mesmo avançando normalmente, aumente `TEMPO_PRESO` ou reduza `VARIACAO_PRESO`. Se demorar demais para perceber que está travado, faça o contrário. A detecção é ignorada quando o sensor não recebe eco útil.

## Calibração dos dois braços

Cada braço deve se mover apenas entre “frente” e “cima”.

1. Grave `teste_montagem.ino`.
2. Digite **6** para o braço esquerdo ou **7** para o direito. O servo escolhido vai para 90°.
3. Com a alimentação desligada, solte o parafuso da haste e encaixe o braço esticado para a frente.
4. Religue e abra novamente a opção correspondente.
5. Teste ângulos em passos de 10°, aproximando-se de 0° ou 180°, até o braço apontar para cima sem forçar o batente.
6. Digite **-1** para sair e registre os dois ângulos no programa principal.

Configuração atual:

```cpp
const int BRACO_ESQ_FRENTE = 90;
const int BRACO_ESQ_CIMA   = 0;
const int BRACO_DIR_FRENTE = 90;
const int BRACO_DIR_CIMA   = 180;
```

Os servos são espelhados, por isso o sentido normalmente é oposto. Se algum braço descer, atingir a roda ou vibrar no fim do curso, interrompa a alimentação e reduza o intervalo antes de testar novamente.

## Controle manual e Wi-Fi

| Constante | Valor atual | Função |
|---|---:|---|
| `PWM_MANUAL` | `180` | Velocidade dos comandos manuais |
| `TEMPO_SEGURANCA_MANUAL` | `600` ms | Para se o celular deixar de renovar o comando |
| `DIST_ANTICOLISAO` | `15.0` cm | Bloqueia somente o avanço manual perto de obstáculo |
| `WIFI_NOME` | `"wally 2.0"` | Nome do ponto de acesso |
| `WIFI_SENHA` | `"12345678"` | Senha do ponto de acesso |

Depois da gravação, confirme que o robô inicia pausado, que a página abre em `http://192.168.4.1` e que soltar uma seta interrompe o movimento. Teste a anticolisão em baixa velocidade e com espaço livre para ré e giros.
