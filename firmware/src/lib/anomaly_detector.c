#include "anomaly_detector.h"
#include <math.h>

void baseline_accumulator_reset(baseline_accumulator_t *acc,
                                uint32_t warmup_samples)
{
    acc->sum = 0.0f;
    acc->sum_sq = 0.0f;
    acc->count = 0;
    acc->warmup_pendientes = warmup_samples;
}

bool baseline_accumulator_add(baseline_accumulator_t *acc, float sample)
{
    /* Política 2: las primeras muestras corresponden al transitorio de
     * arranque (autocalentamiento, arranque del motor) y no representan el
     * régimen estable de la máquina. Se descartan. */
    if (acc->warmup_pendientes > 0) {
        acc->warmup_pendientes--;
        return false;
    }

    acc->sum += sample;
    acc->sum_sq += sample * sample;
    acc->count++;
    return true;
}

/* Estadística sin el piso mínimo aplicado. El piso es una política de
 * detección; para juzgar si un nivel es estable hace falta la dispersión
 * real. */
static bool stats_crudas(const baseline_accumulator_t *acc,
                         float *mean, float *stddev)
{
    if (acc->count < 2) {
        return false;
    }

    float m = acc->sum / (float)acc->count;
    float variance = (acc->sum_sq / (float)acc->count) - (m * m);

    if (variance < 0.0f) {
        variance = 0.0f; /* cancelación numérica con varianza real ~0 */
    }

    *mean = m;
    *stddev = sqrtf(variance);
    return true;
}

baseline_t baseline_accumulator_finalize(const baseline_accumulator_t *acc)
{
    baseline_t b = { .mean = 0.0f, .stddev = 0.0f, .valid = false };
    float mean, stddev;

    if (!stats_crudas(acc, &mean, &stddev)) {
        return b;
    }

    /* Política 1: piso mínimo relativo a la media. Una señal demasiado
     * estable produciría una banda más estrecha que su propia fluctuación
     * natural, y dispararía sin parar. El piso absoluto solo cubre el caso
     * de media ~0, donde el relativo no aporta nada. */
    float piso_relativo = BASELINE_MIN_STDDEV_FRACTION * fabsf(mean);
    float piso = (piso_relativo > BASELINE_MIN_STDDEV_ABS)
                 ? piso_relativo : BASELINE_MIN_STDDEV_ABS;

    if (stddev < piso) {
        stddev = piso;
    }

    b.mean   = mean;
    b.stddev = stddev;
    b.valid  = true;
    return b;
}

bool anomaly_z_score_check(const baseline_t *baseline, float sample,
                            float z_threshold, float *out_z)
{
    /* finalize() ya garantiza stddev >= BASELINE_MIN_STDDEV_ABS > 0, pero
     * se protege igualmente por si llega una línea base construida a mano. */
    float stddev = baseline->stddev;

    if (stddev < BASELINE_MIN_STDDEV_ABS) {
        stddev = BASELINE_MIN_STDDEV_ABS;
    }

    float z = (sample - baseline->mean) / stddev;

    if (out_z) {
        *out_z = z;
    }

    return fabsf(z) > z_threshold;
}

void anomaly_debounce_reset(anomaly_debounce_t *d, uint32_t requeridas)
{
    d->consecutivas = 0;
    d->requeridas = (requeridas == 0) ? 1 : requeridas;
}

bool anomaly_debounce_update(anomaly_debounce_t *d, bool muestra_anomala)
{
    /* Política 3: una muestra normal rompe la racha. Solo una desviación
     * sostenida —que es como se manifiesta la degradación real— llega a
     * declararse anomalía. */
    if (!muestra_anomala) {
        d->consecutivas = 0;
        return false;
    }

    if (d->consecutivas < d->requeridas) {
        d->consecutivas++;
    }

    return d->consecutivas >= d->requeridas;
}

void baseline_relearn_reset(baseline_relearn_t *r, uint32_t requeridas)
{
    baseline_accumulator_reset(&r->acc, 0);
    r->requeridas = (requeridas < 2) ? 2 : requeridas; /* hacen falta >=2 */
    r->activo = false;
}

bool baseline_relearn_update(baseline_relearn_t *r, float sample,
                             bool fuera_de_banda, baseline_t *nueva)
{
    /* Una sola muestra dentro de banda significa que la máquina volvió a su
     * normalidad: no hubo cambio de carga, solo una perturbación pasajera. */
    if (!fuera_de_banda) {
        r->activo = false;
        baseline_accumulator_reset(&r->acc, 0);
        return false;
    }

    if (!r->activo) {
        r->activo = true;
        baseline_accumulator_reset(&r->acc, 0);
    }

    baseline_accumulator_add(&r->acc, sample);

    if (r->acc.count < r->requeridas) {
        return false;
    }

    /* Hay suficientes muestras fuera de banda. La pregunta ahora es si
     * forman un nivel nuevo y ESTABLE (cambio de máquina) o siguen
     * moviéndose (degradación en curso). */
    float mean, stddev;

    if (!stats_crudas(&r->acc, &mean, &stddev)) {
        return false;
    }

    float dispersion_max = BASELINE_RELEARN_STABILITY_FRACTION * fabsf(mean);

    if (dispersion_max < BASELINE_MIN_STDDEV_ABS) {
        dispersion_max = BASELINE_MIN_STDDEV_ABS;
    }

    if (stddev > dispersion_max) {
        /* Sigue derivando: no es una máquina nueva, es una señal inestable.
         * Se descarta el candidato y se vuelve a observar, manteniendo la
         * alarma activa mientras tanto. */
        baseline_accumulator_reset(&r->acc, 0);
        return false;
    }

    *nueva = baseline_accumulator_finalize(&r->acc);
    r->activo = false;
    baseline_accumulator_reset(&r->acc, 0);
    return nueva->valid;
}
