#include <zephyr/ztest.h>
#include "anomaly_detector.h"

ZTEST(anomaly_detector, test_baseline_media_y_desviacion)
{
    /* Referencia calculada independientemente: mean=3.0, stddev=sqrt(2) */
    baseline_accumulator_t acc;
    baseline_accumulator_reset(&acc);

    float samples[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    for (int i = 0; i < 5; i++) {
        baseline_accumulator_add(&acc, samples[i]);
    }

    baseline_t b = baseline_accumulator_finalize(&acc);

    zassert_true(b.valid, "la línea base debería ser válida con 5 muestras");
    zassert_within(b.mean, 3.0f, 0.001f, "mean esperado 3.0, obtuve %f", (double)b.mean);
    zassert_within(b.stddev, 1.41421356f, 0.001f,
                   "stddev esperado sqrt(2), obtuve %f", (double)b.stddev);
}

ZTEST(anomaly_detector, test_baseline_insuficientes_muestras)
{
    baseline_accumulator_t acc;
    baseline_accumulator_reset(&acc);
    baseline_accumulator_add(&acc, 10.0f);

    baseline_t b = baseline_accumulator_finalize(&acc);

    zassert_false(b.valid, "con 1 sola muestra la línea base no debe ser válida");
}

ZTEST(anomaly_detector, test_zscore_dentro_del_umbral_no_es_anomalia)
{
    baseline_t b = { .mean = 3.0f, .stddev = 1.41421356f, .valid = true };
    float z;

    bool anomalo = anomaly_z_score_check(&b, 3.5f, 3.0f, &z);

    zassert_false(anomalo, "una lectura cercana a la media no debería disparar alarma");
}

ZTEST(anomaly_detector, test_zscore_fuera_del_umbral_es_anomalia)
{
    baseline_t b = { .mean = 3.0f, .stddev = 1.41421356f, .valid = true };
    float z;

    /* 3.0 + 10*stddev está muy lejos de la línea base */
    bool anomalo = anomaly_z_score_check(&b, 3.0f + 10.0f * 1.41421356f, 3.0f, &z);

    zassert_true(anomalo, "una lectura a 10 desviaciones estándar debe ser anomalía");
    zassert_true(z > 3.0f, "el z-score debe ser positivo y mayor al umbral");
}

ZTEST(anomaly_detector, test_zscore_linea_base_plana_no_divide_por_cero)
{
    /* stddev=0 — p.ej. sin sensor conectado durante calibración */
    baseline_t b = { .mean = 0.0f, .stddev = 0.0f, .valid = true };
    float z;

    /* La misma lectura que la media: no debe ser anomalía */
    zassert_false(anomaly_z_score_check(&b, 0.0f, 3.0f, &z),
                  "una lectura igual a la media no debería ser anomalía aunque stddev=0");

    /* Cualquier desviación real con stddev=0 debe detectarse (piso mínimo) */
    zassert_true(anomaly_z_score_check(&b, 5.0f, 3.0f, &z),
                 "una desviación real con línea base plana debe detectarse, no crashear");
}

ZTEST_SUITE(anomaly_detector, NULL, NULL, NULL, NULL, NULL);
