#include "anomaly_detector.h"
#include <math.h>

#define MIN_STDDEV 0.0001f

void baseline_accumulator_reset(baseline_accumulator_t *acc)
{
    acc->sum = 0.0f;
    acc->sum_sq = 0.0f;
    acc->count = 0;
}

void baseline_accumulator_add(baseline_accumulator_t *acc, float sample)
{
    acc->sum += sample;
    acc->sum_sq += sample * sample;
    acc->count++;
}

baseline_t baseline_accumulator_finalize(const baseline_accumulator_t *acc)
{
    baseline_t b = { .mean = 0.0f, .stddev = 0.0f, .valid = false };

    if (acc->count < 2) {
        return b;
    }

    float mean     = acc->sum / (float)acc->count;
    float variance = (acc->sum_sq / (float)acc->count) - (mean * mean);
    if (variance < 0.0f) {
        variance = 0.0f; /* cancelación numérica con varianza real ~0 */
    }

    b.mean   = mean;
    b.stddev = sqrtf(variance);
    b.valid  = true;
    return b;
}

bool anomaly_z_score_check(const baseline_t *baseline, float sample,
                            float z_threshold, float *out_z)
{
    float stddev = baseline->stddev;
    if (stddev < MIN_STDDEV) {
        stddev = MIN_STDDEV;
    }

    float z = (sample - baseline->mean) / stddev;
    if (out_z) {
        *out_z = z;
    }

    return fabsf(z) > z_threshold;
}
