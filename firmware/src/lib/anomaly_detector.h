#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

/* Detector estadístico de anomalías (z-score sobre línea base calibrada
 * por máquina, FR3). Sin hardware, sin dependencias de Zephyr —
 * testeable en host.
 *
 * La validación en hardware real destapó dos problemas que la teoría no
 * anticipa, y que motivan las tres políticas de abajo:
 *
 *   1. Una máquina muy estable produce una desviación típica minúscula
 *      (medido: 0.003 A sobre 0.603 A). Con un umbral de 3 sigma, la banda
 *      "normal" queda en ±0.009 A, más estrecha que la propia fluctuación
 *      natural de la carga (±0.012 A observados) -> falsas alarmas
 *      garantizadas. El z-score no está mal: es demasiado sensible para ser
 *      útil.
 *
 *   2. Calibrar nada más arrancar captura un transitorio, no el régimen
 *      estable. Medido: la temperatura subía de 33.24 a 33.85 °C por
 *      autocalentamiento del sensor; la línea base salió con media 33.34,
 *      de modo que el valor estable REAL quedaba a z=3.6 -> anomalía
 *      permanente desde el primer momento.
 */

/* --- Política 1: piso mínimo RELATIVO de la desviación típica ------------
 *
 * Una desviación por debajo de este porcentaje de la media se considera
 * "ruido de medida", no información. Evita que una señal casi perfecta
 * genere alarmas por fluctuaciones irrelevantes.
 */
#define BASELINE_MIN_STDDEV_FRACTION  0.02f   /* 2 % de la media */

/* Piso absoluto, solo para evitar división por cero cuando la media
 * también es ~0 (p. ej. un sensor desconectado durante la calibración). */
#define BASELINE_MIN_STDDEV_ABS       0.0001f

typedef struct {
    float    sum;
    float    sum_sq;
    uint32_t count;
    uint32_t warmup_pendientes;  /* muestras a descartar antes de acumular */
} baseline_accumulator_t;

typedef struct {
    float mean;
    float stddev;   /* ya incluye el piso mínimo aplicado */
    bool  valid;
} baseline_t;

/* --- Política 2: descartar el transitorio de arranque --------------------
 *
 * `warmup_samples` indica cuántas muestras iniciales se ignoran antes de
 * empezar a acumular. Un 0 desactiva el descarte.
 */
void baseline_accumulator_reset(baseline_accumulator_t *acc,
                                uint32_t warmup_samples);

/* Descarta la muestra si aún quedan muestras de calentamiento; si no, la
 * acumula. Devuelve true si la muestra se acumuló de verdad. */
bool baseline_accumulator_add(baseline_accumulator_t *acc, float sample);

/* Requiere al menos 2 muestras acumuladas; si no, devuelve valid=false.
 * La desviación devuelta ya tiene aplicado el piso mínimo relativo. */
baseline_t baseline_accumulator_finalize(const baseline_accumulator_t *acc);

/* z = (sample - mean) / stddev. Devuelve true si |z| > z_threshold. */
bool anomaly_z_score_check(const baseline_t *baseline, float sample,
                            float z_threshold, float *out_z);

/* --- Política 3: exigir persistencia ------------------------------------
 *
 * Una sola muestra fuera de banda puede ser ruido; una degradación real es
 * lenta y sostenida. Se exige que N muestras CONSECUTIVAS sean anómalas
 * antes de declarar la anomalía, que es lo que hacen los sistemas
 * industriales. Una sola muestra normal reinicia la cuenta.
 */
typedef struct {
    uint32_t consecutivas;
    uint32_t requeridas;
} anomaly_debounce_t;

void anomaly_debounce_reset(anomaly_debounce_t *d, uint32_t requeridas);

/* Alimenta el resultado crudo del z-score. Devuelve true solo cuando se
 * alcanza el número de muestras anómalas consecutivas exigido. */
bool anomaly_debounce_update(anomaly_debounce_t *d, bool muestra_anomala);

/* --- Política 4: reaprendizaje ante un cambio de máquina ----------------
 *
 * La línea base es por máquina (FR3): comparamos una máquina consigo misma.
 * Pero el nodo no tiene forma de saber que le han cambiado la carga, y sin
 * esto se quedaría alarmando para siempre contra una línea base obsoleta.
 *
 * La tentación sería una línea base adaptativa, pero eso rompe el propósito
 * del proyecto: una avería progresa despacio, y una línea base que se mueve
 * sola la absorbería sin avisar nunca.
 *
 * La distinción está en la FORMA de la desviación, no en su tamaño:
 *
 *   cambio de máquina -> salto brusco a un nivel nuevo, y ESTABLE ahí
 *   degradación       -> deriva lenta y sostenida, que no se estabiliza
 *
 * Por eso solo se readopta la línea base cuando las muestras anómalas son
 * además estables ENTRE SÍ. Una señal que sigue moviéndose nunca se adopta,
 * y se mantiene la alarma.
 */

/* Dispersión máxima (respecto a su propia media) que puede tener el nuevo
 * nivel para considerarse "estable" y adoptarse como línea base. */
#define BASELINE_RELEARN_STABILITY_FRACTION  0.05f   /* 5 % */

typedef struct {
    baseline_accumulator_t acc;
    uint32_t               requeridas;
    bool                   activo;
} baseline_relearn_t;

void baseline_relearn_reset(baseline_relearn_t *r, uint32_t requeridas);

/* Se alimenta con cada muestra y con el veredicto crudo del z-score.
 * Devuelve true, y rellena *nueva, solo cuando la señal lleva `requeridas`
 * muestras fuera de banda y además estable en un nivel nuevo. */
bool baseline_relearn_update(baseline_relearn_t *r, float sample,
                             bool fuera_de_banda, baseline_t *nueva);

#endif
