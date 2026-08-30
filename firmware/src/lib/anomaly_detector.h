#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

/* Detector estadístico de anomalías (z-score sobre línea base calibrada
 * por máquina, FR3). Sin hardware, sin dependencias de Zephyr —
 * testeable en host. */

typedef struct {
    float    sum;
    float    sum_sq;
    uint32_t count;
} baseline_accumulator_t;

typedef struct {
    float mean;
    float stddev;
    bool  valid;
} baseline_t;

void baseline_accumulator_reset(baseline_accumulator_t *acc);
void baseline_accumulator_add(baseline_accumulator_t *acc, float sample);

/* Requiere al menos 2 muestras acumuladas; si no, devuelve valid=false. */
baseline_t baseline_accumulator_finalize(const baseline_accumulator_t *acc);

/* z = (sample - mean) / stddev. Devuelve true si |z| > z_threshold.
 * Si stddev es ~0 (línea base plana — p.ej. sin sensor conectado durante
 * calibración), se usa un piso mínimo para evitar división por cero. */
bool anomaly_z_score_check(const baseline_t *baseline, float sample,
                            float z_threshold, float *out_z);

#endif
