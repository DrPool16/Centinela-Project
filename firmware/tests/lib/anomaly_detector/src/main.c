#include <zephyr/ztest.h>
#include "anomaly_detector.h"

/* ── Acumulación y estadística básica ────────────────────────────────── */

ZTEST(anomaly_detector, test_baseline_media_y_desviacion)
{
    /* Referencia calculada independientemente: mean=3.0, stddev=sqrt(2).
     * sqrt(2) queda muy por encima del piso mínimo (2% de 3.0 = 0.06), así
     * que aquí el piso no interfiere. */
    baseline_accumulator_t acc;
    baseline_accumulator_reset(&acc, 0);

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
    baseline_accumulator_reset(&acc, 0);
    baseline_accumulator_add(&acc, 10.0f);

    baseline_t b = baseline_accumulator_finalize(&acc);

    zassert_false(b.valid, "con 1 sola muestra la línea base no debe ser válida");
}

/* ── Política 1: piso mínimo relativo ────────────────────────────────── */

ZTEST(anomaly_detector, test_piso_minimo_relativo_a_la_media)
{
    /* Señal perfectamente estable: varianza real 0. Sin piso relativo, la
     * banda sería de ancho ~0 y cualquier fluctuación dispararía alarma.
     * Caso medido en hardware: corriente 0.603 A con stddev 0.003. */
    baseline_accumulator_t acc;
    baseline_accumulator_reset(&acc, 0);

    for (int i = 0; i < 10; i++) {
        baseline_accumulator_add(&acc, 100.0f);
    }

    baseline_t b = baseline_accumulator_finalize(&acc);

    zassert_true(b.valid, NULL);
    zassert_within(b.mean, 100.0f, 0.001f, NULL);
    zassert_within(b.stddev, 2.0f, 0.001f,
                   "el piso debe ser el 2%% de la media (2.0), obtuve %f",
                   (double)b.stddev);
}

ZTEST(anomaly_detector, test_piso_relativo_evita_falsa_alarma)
{
    /* Reproduce el caso real: media 0.603 A, fluctuación natural ±0.012 A.
     * Con la desviación medida (0.003) esa fluctuación daba z=4 y alarma;
     * con el piso relativo (2% = 0.01206) queda dentro de las 3 sigma. */
    baseline_accumulator_t acc;
    baseline_accumulator_reset(&acc, 0);

    float muestras[] = {0.603f, 0.602f, 0.604f, 0.603f, 0.602f, 0.604f};
    for (int i = 0; i < 6; i++) {
        baseline_accumulator_add(&acc, muestras[i]);
    }

    baseline_t b = baseline_accumulator_finalize(&acc);
    float z;

    zassert_false(anomaly_z_score_check(&b, 0.615f, 3.0f, &z),
                  "una fluctuación normal no debe ser anomalía (z=%f)", (double)z);
    zassert_true(anomaly_z_score_check(&b, 0.400f, 3.0f, &z),
                 "una caída real de corriente sí debe detectarse (z=%f)", (double)z);
}

/* ── Política 2: descartar el transitorio de arranque ────────────────── */

ZTEST(anomaly_detector, test_warmup_descarta_las_primeras_muestras)
{
    baseline_accumulator_t acc;
    baseline_accumulator_reset(&acc, 3);

    /* Las 3 primeras simulan el calentamiento y deben ignorarse. */
    zassert_false(baseline_accumulator_add(&acc, 1000.0f), "muestra 1 debe descartarse");
    zassert_false(baseline_accumulator_add(&acc, 2000.0f), "muestra 2 debe descartarse");
    zassert_false(baseline_accumulator_add(&acc, 3000.0f), "muestra 3 debe descartarse");

    zassert_equal(acc.count, 0, "nada debe haberse acumulado durante el calentamiento");

    zassert_true(baseline_accumulator_add(&acc, 5.0f), "muestra 4 debe acumularse");
    zassert_true(baseline_accumulator_add(&acc, 7.0f), "muestra 5 debe acumularse");

    baseline_t b = baseline_accumulator_finalize(&acc);

    zassert_true(b.valid, NULL);
    zassert_within(b.mean, 6.0f, 0.001f,
                   "la media debe ignorar el transitorio, obtuve %f", (double)b.mean);
}

ZTEST(anomaly_detector, test_warmup_cero_no_descarta_nada)
{
    baseline_accumulator_t acc;
    baseline_accumulator_reset(&acc, 0);

    zassert_true(baseline_accumulator_add(&acc, 1.0f), "sin warmup todo se acumula");
    zassert_equal(acc.count, 1, NULL);
}

/* ── z-score ─────────────────────────────────────────────────────────── */

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
    /* Línea base construida a mano con stddev=0 (no viene de finalize). */
    baseline_t b = { .mean = 0.0f, .stddev = 0.0f, .valid = true };
    float z;

    zassert_false(anomaly_z_score_check(&b, 0.0f, 3.0f, &z),
                  "una lectura igual a la media no debería ser anomalía aunque stddev=0");

    zassert_true(anomaly_z_score_check(&b, 5.0f, 3.0f, &z),
                 "una desviación real con línea base plana debe detectarse, no crashear");
}

/* ── Política 3: persistencia ────────────────────────────────────────── */

ZTEST(anomaly_detector, test_debounce_exige_muestras_consecutivas)
{
    anomaly_debounce_t d;
    anomaly_debounce_reset(&d, 3);

    zassert_false(anomaly_debounce_update(&d, true), "1 de 3 no basta");
    zassert_false(anomaly_debounce_update(&d, true), "2 de 3 no basta");
    zassert_true(anomaly_debounce_update(&d, true), "3 de 3 debe declarar anomalía");
    zassert_true(anomaly_debounce_update(&d, true), "debe seguir declarada mientras persista");
}

ZTEST(anomaly_detector, test_debounce_una_muestra_normal_rompe_la_racha)
{
    anomaly_debounce_t d;
    anomaly_debounce_reset(&d, 3);

    anomaly_debounce_update(&d, true);
    anomaly_debounce_update(&d, true);

    zassert_false(anomaly_debounce_update(&d, false), "una muestra normal no declara nada");

    /* Tras romperse la racha hay que volver a empezar desde cero. */
    zassert_false(anomaly_debounce_update(&d, true), "la cuenta debe haberse reiniciado");
    zassert_false(anomaly_debounce_update(&d, true), NULL);
    zassert_true(anomaly_debounce_update(&d, true), "3 consecutivas de nuevo sí declara");
}

ZTEST(anomaly_detector, test_debounce_ruido_aislado_no_dispara)
{
    /* Patrón típico de ruido: picos sueltos entre lecturas normales. */
    anomaly_debounce_t d;
    anomaly_debounce_reset(&d, 3);

    bool patron[] = {true, false, true, false, true, true, false, true};

    for (int i = 0; i < 8; i++) {
        zassert_false(anomaly_debounce_update(&d, patron[i]),
                      "el ruido aislado no debe declarar anomalía (paso %d)", i);
    }
}

ZTEST(anomaly_detector, test_debounce_cero_se_trata_como_uno)
{
    /* Pedir 0 muestras no tiene sentido: debe comportarse como 1. */
    anomaly_debounce_t d;
    anomaly_debounce_reset(&d, 0);

    zassert_true(anomaly_debounce_update(&d, true),
                 "con requeridas=0 la primera muestra anómala debe declarar");
}

/* ── Política 4: reaprendizaje ante cambio de máquina ────────────────── */

ZTEST(anomaly_detector, test_relearn_adopta_un_nivel_nuevo_estable)
{
    /* Escenario: se cambia el abanico (0.6 A) por otro aparato (2.0 A).
     * Tras varias muestras estables en el nivel nuevo, se readopta. */
    baseline_relearn_t r;
    baseline_relearn_reset(&r, 5);
    baseline_t nueva;

    float nivel_nuevo[] = {2.00f, 2.01f, 1.99f, 2.00f, 2.01f};

    for (int i = 0; i < 4; i++) {
        zassert_false(baseline_relearn_update(&r, nivel_nuevo[i], true, &nueva),
                      "no debe adoptar antes de reunir las muestras exigidas");
    }

    zassert_true(baseline_relearn_update(&r, nivel_nuevo[4], true, &nueva),
                 "con 5 muestras estables debe adoptar la línea base nueva");
    zassert_within(nueva.mean, 2.0f, 0.02f,
                   "la media nueva debe ser ~2.0, obtuve %f", (double)nueva.mean);
    zassert_true(nueva.valid, NULL);
}

ZTEST(anomaly_detector, test_relearn_no_adopta_una_deriva)
{
    /* Escenario crítico: degradación progresiva. Las muestras están fuera
     * de banda, pero NO son estables entre sí — siguen subiendo. Adoptarlas
     * como normalidad nueva sería absorber la avería y dejar de avisar. */
    baseline_relearn_t r;
    baseline_relearn_reset(&r, 5);
    baseline_t nueva;

    float deriva[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f};

    for (int i = 0; i < 8; i++) {
        zassert_false(baseline_relearn_update(&r, deriva[i], true, &nueva),
                      "una deriva no debe readoptarse como normalidad (paso %d)", i);
    }
}

ZTEST(anomaly_detector, test_relearn_se_cancela_al_volver_a_la_normalidad)
{
    /* Una perturbación pasajera no debe acabar cambiando la línea base. */
    baseline_relearn_t r;
    baseline_relearn_reset(&r, 3);
    baseline_t nueva;

    baseline_relearn_update(&r, 2.0f, true, &nueva);
    baseline_relearn_update(&r, 2.0f, true, &nueva);

    /* Vuelve dentro de banda: se descarta todo lo acumulado. */
    zassert_false(baseline_relearn_update(&r, 0.6f, false, &nueva), NULL);

    /* Y hay que volver a empezar de cero. */
    zassert_false(baseline_relearn_update(&r, 2.0f, true, &nueva),
                  "la cuenta debe haberse reiniciado");
    zassert_false(baseline_relearn_update(&r, 2.0f, true, &nueva), NULL);
    zassert_true(baseline_relearn_update(&r, 2.0f, true, &nueva),
                 "3 consecutivas y estables de nuevo sí adoptan");
}

ZTEST(anomaly_detector, test_relearn_la_nueva_base_reconoce_su_propio_nivel)
{
    /* Tras readoptar, el nivel nuevo debe considerarse normal, y el nivel
     * viejo pasar a ser la anomalía. Es la prueba de que el reaprendizaje
     * sirve de algo. */
    baseline_relearn_t r;
    baseline_relearn_reset(&r, 4);
    baseline_t nueva;
    float z;

    float nivel[] = {2.00f, 2.01f, 1.99f, 2.00f};

    for (int i = 0; i < 4; i++) {
        baseline_relearn_update(&r, nivel[i], true, &nueva);
    }

    zassert_true(nueva.valid, "debería haberse adoptado una línea base");
    zassert_false(anomaly_z_score_check(&nueva, 2.00f, 3.0f, &z),
                  "el nivel nuevo ya es lo normal");
    zassert_true(anomaly_z_score_check(&nueva, 0.60f, 3.0f, &z),
                 "el nivel viejo ahora debe ser la anomalía");
}

ZTEST_SUITE(anomaly_detector, NULL, NULL, NULL, NULL, NULL);
