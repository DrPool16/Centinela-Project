# Guía de interpretación de las medidas

Este documento explica **cómo leer los números que imprime el nodo** y, sobre
todo, cómo saber si son creíbles. Está escrito para alguien sin formación en
electrónica: cada término se define cuando aparece por primera vez.

> La regla de fondo: un número sin contexto no es una medida. Una medida es
> un número **más** una forma de comprobar que tiene sentido.

---

## 1. La cadena de medida

Antes de interpretar nada, conviene saber por dónde pasa la señal:

```
Corriente real       Pinza            Señal          Conversor        Números
por el cable    ->  SCT-013     ->   eléctrica  ->    ADS1115    ->   crudos
                 (transformador)     (voltaje)      (digitaliza)     (cuentas)
```

1. **La pinza (SCT-013)** se abraza alrededor de **un solo conductor** de la
   carga. Es un transformador de corriente: el campo magnético que genera la
   corriente del cable induce una señal proporcional en la pinza. Si abrazas
   los dos conductores (fase y neutro) los campos se cancelan y siempre
   medirás ~0 A. Es el error más común.
2. **La pinza entrega voltaje**, no corriente (la variante que usamos lleva su
   resistencia interna). Concretamente **1 voltio cuando pasan 30 amperios**.
   Esa proporción es la que convierte voltios en amperios.
3. **El ADS1115** es un *conversor analógico-digital* (ADC): traduce ese
   voltaje a un número entero. No entiende de voltios, solo cuenta.
4. El firmware deshace la cadena hacia atrás: cuentas → voltios → amperios.

---

## 2. Qué significa cada campo

Ejemplo real medido en la placa, con un abanico encendido:

```
[120] raw: offset=    2.4  min=  -437  max=   438  pico-pico=  875
      Vrms= 0.01964 V   Corriente=  0.589 A   Potencia~   64.8 W  (200/200 ok)
```

### `raw` (cuentas)

Los números crudos que devuelve el ADC, **sin convertir**. El ADS1115 usa 16
bits con signo: el rango va de −32768 a +32767. Ese rango completo representa
el margen de voltaje configurado (±2.048 V), así que:

```
1 cuenta = 2.048 V / 32768 = 62.5 microvoltios (µV)
```

Un microvoltio es una millonésima de voltio. Trabajar en cuentas evita
arrastrar decimales por todo el firmware.

### `offset`

**El "cero" real de la señal.** Idealmente, sin corriente la lectura debería
ser exactamente 0, pero siempre hay una pequeña desviación por tolerancias del
componente y ruido.

El firmware lo mide en vez de asumirlo: toma 200 muestras y calcula su
promedio. Como una señal alterna sube y baja de forma simétrica, su promedio
es su centro.

**Por qué importa:** si dieras por hecho que el cero es 0 y en realidad fuera
50, ese error se colaría en cada cálculo posterior. Medir el offset y restarlo
elimina ese sesgo.

- **Valor esperado:** cercano a 0 (unas pocas cuentas). En el ejemplo: `2.4`.
- **Si es grande** (cientos de cuentas): hay una tensión continua parásita en
  la entrada — revisa el cableado.

### `min`, `max`, `pico-pico`

El valor más bajo, el más alto, y la diferencia entre ambos dentro de la
tanda de 200 muestras.

La corriente de la red es **alterna** (AC): oscila 60 veces por segundo,
subiendo y bajando de forma continua. Por eso una sola lectura instantánea no
sirve de nada — podría haberte pillado justo en el cruce por cero.

- **Es el indicador más directo de que la señal llega.** Con la carga apagada,
  `min` y `max` se quedan pegados cerca de cero. Con carga, se separan.
- **Deben ser aproximadamente simétricos** respecto al offset. En el ejemplo,
  −437 y +438: casi perfecto.
- **Si `min` o `max` se clavan en −32768 o +32767**, la señal está
  **saturando**: excede el rango del ADC y se está recortando. La medida ya no
  es válida (ver sección 5).

### `Vrms`

**RMS** significa *Root Mean Square* (raíz de la media de los cuadrados). Es
la forma correcta de resumir una señal alterna en un solo número.

El promedio directo de una señal alterna es cero (lo que sube, baja), así que
no sirve. El RMS eleva cada muestra al cuadrado —con lo que todo se vuelve
positivo—, promedia, y saca la raíz:

```
RMS = raíz_cuadrada( promedio( (muestra - offset)² ) )
```

El resultado es **el valor de una señal continua que produciría el mismo
calor**. Por eso es la magnitud que usan los multímetros y las facturas de
la luz.

Fíjate en el `- offset` de la fórmula: el RMS se toma respecto al centro real
de la señal, no respecto a cero. Si no se restara, cualquier desviación
continua de la cadena de medida se sumaría a la lectura **como si fuera
corriente**, inflando el resultado sin que nada lo delate.

### `Corriente`

`Vrms` convertido a amperios con la proporción de la pinza:

```
Corriente (A) = Vrms (V) × 30
```

El `30` viene del modelo SCT-013-**030**: 30 amperios por cada voltio. Si
usaras otra pinza, ese número cambia.

### `Potencia`

Estimación de la potencia aparente:

```
Potencia (W) = Corriente (A) × 110 V
```

Los 110 V son la tensión nominal de la red local, **asumida, no medida** —
este nodo no mide voltaje de red. Por eso el símbolo `~`: es orientativa.

> **Limitación honesta:** esto es *potencia aparente* (VA), no potencia real
> (W). En cargas con motor —como un abanico— ambas difieren por el llamado
> *factor de potencia*. La cifra sirve para comparar el consumo de una misma
> máquina consigo misma a lo largo del tiempo, que es justo lo que necesita el
> mantenimiento predictivo, pero no es una medida de facturación.

### `(200/200 ok)`

Lecturas válidas de cada una de las dos pasadas. **Es el indicador de salud
del bus I2C.** Si baja de 200, hay transacciones fallando y las medidas
pierden fiabilidad.

---

## 3. Cómo verificar que los números son creíbles

Esta es la parte que convierte "leer números" en "medir".

### La prueba del factor de cresta

Para una onda senoidal limpia, el valor de pico y el RMS guardan una relación
fija: **pico = RMS × √2** (≈ 1.414). Esa proporción se llama *factor de
cresta*.

Aplicándolo al ejemplo real:

```
pico = 438 cuentas × 62.5 µV      = 0.02738 V
RMS teórico = 0.02738 / 1.414     = 0.01936 V
RMS medido                        = 0.01964 V
diferencia                        = 1.5 %
```

Coinciden. Eso confirma **dos cosas a la vez**: que el cálculo de RMS está
bien implementado, y que la señal que entra es una senoidal limpia.

- **Factor de cresta ≈ 1.41** → señal senoidal sana.
- **Bastante mayor** (2, 3...) → señal con picos: ruido, o una carga que no
  consume de forma senoidal (fuentes conmutadas, variadores).
- **Bastante menor** (cerca de 1) → señal recortada, probablemente saturando.

### La prueba de encender y apagar

La más simple y la más contundente: **apaga la carga**. La corriente debe caer
a casi cero y el `pico-pico` colapsar. Si el número no se mueve al apagar,
no estás midiendo la carga (pinza mal puesta, o abrazando ambos conductores).

### La prueba de estabilidad

Con una carga constante, lecturas consecutivas deben variar poco. En el
ejemplo: `0.590`, `0.589`, `0.589` — menos del 0.2%. Si saltan sin que la
carga cambie, hay ruido o un problema de contacto.

---

## 4. Señales de que algo va mal

| Lo que ves | Qué suele significar |
|---|---|
| `pico-pico` casi 0 con la carga encendida | La pinza abraza los dos conductores (campos cancelados) o no está cerrada |
| Corriente que no cambia al apagar la carga | La pinza no está sobre el conductor correcto |
| `min`/`max` clavados en ±32767 | Señal saturada: el rango del ADC es insuficiente (ver sección 5) |
| `offset` de cientos de cuentas | Tensión continua parásita en la entrada |
| `(150/200 ok)` o menos | El bus I2C está fallando: cableado, pull-ups o direcciones |
| Valores que saltan sin patrón | Ruido eléctrico, falso contacto, o masa no compartida |
| Factor de cresta muy distinto de 1.41 | La señal no es senoidal: ruido o carga no lineal |

---

## 5. El compromiso rango-resolución (PGA)

El **PGA** (*Programmable Gain Amplifier*) fija qué margen de voltaje
representa el rango completo del ADC. Es un compromiso directo:

| PGA | Rango | µV por cuenta | Corriente máxima medible |
|---|---|---|---|
| ±2.048 V | amplio | 62.5 | ~43 A (limitado a 30 A por la pinza) |
| ±0.256 V | estrecho | 7.8 | ~5.4 A |

En el ejemplo real, el pico es de 438 cuentas sobre 32768: **se está usando el
1.3% del rango disponible**. El PGA está configurado para los 30 A que soporta
la pinza, pero la carga medida es de 0.6 A.

Bajando el PGA a ±0.256 V se ganarían **8× más resolución** (de ~1.9 mA por
cuenta a ~0.23 mA), a costa de saturar por encima de ~5.4 A.

**Cuál elegir depende del caso de uso:** para monitorizar un motor industrial
de varios amperios, el rango amplio es correcto. Para detectar variaciones
finas en una carga pequeña —que es justo lo que busca la detección de
anomalías por z-score—, el rango estrecho da mucha más sensibilidad.

Queda documentado como mejora pendiente, no aplicada: cambiarlo exige conocer
de antemano el rango de corriente de la máquina vigilada.

---

## 6. Por qué se toman 200 muestras y no una

A 60 Hz, un ciclo completo de la red dura 16.7 ms. El ADS1115 está
configurado a **860 muestras por segundo**, lo que da unas **14 muestras por
ciclo**.

Con 200 muestras se cubren unos 14 ciclos completos, suficiente para que el
RMS sea estable y no dependa de en qué punto de la onda empezó el muestreo.

Lo que limita el ritmo real no es el ADC, sino el bus I2C: entre el sondeo
del bit de "conversión lista" y la lectura del resultado, cada muestra sale a
unos 2 ms, es decir ~450 muestras por segundo efectivas.

> El driver del firmware (`firmware/src/drivers/ads1115.c`) usaba
> originalmente 128 SPS, que a 60 Hz daba **menos de 2 muestras por ciclo** —
> por debajo del límite de Nyquist, con el aliasing correspondiente. Se
> corrigió a 860 SPS tras la validación en hardware.

El driver usa 64 muestras por lectura, que a ~450 muestras/s cubren unos 8
ciclos de red.
