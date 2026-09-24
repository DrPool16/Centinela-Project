#include <zephyr/ztest.h>
#include "record_index.h"

/* Almacén falso: los `escritos` primeros registros están ocupados y el
 * resto vacíos. Cuenta las llamadas para verificar que el coste es
 * logarítmico y no lineal. */
struct almacen_falso {
    uint32_t escritos;
    uint32_t llamadas;
};

static bool vacio(uint32_t index, void *ctx)
{
    struct almacen_falso *a = ctx;

    a->llamadas++;
    return index >= a->escritos;
}

static uint32_t contar(uint32_t max, uint32_t escritos, uint32_t *llamadas)
{
    struct almacen_falso a = { .escritos = escritos, .llamadas = 0 };
    uint32_t n = record_index_find_count(max, vacio, &a);

    if (llamadas) {
        *llamadas = a.llamadas;
    }
    return n;
}

ZTEST(record_index, test_memoria_vacia)
{
    zassert_equal(contar(1000, 0, NULL), 0,
                  "sin registros escritos la frontera está en 0");
}

ZTEST(record_index, test_un_solo_registro)
{
    zassert_equal(contar(1000, 1, NULL), 1, NULL);
}

ZTEST(record_index, test_memoria_llena)
{
    zassert_equal(contar(1000, 1000, NULL), 1000,
                  "con todo escrito la frontera es max_records");
}

ZTEST(record_index, test_frontera_en_el_penultimo)
{
    zassert_equal(contar(1000, 999, NULL), 999, NULL);
}

ZTEST(record_index, test_posiciones_intermedias)
{
    /* Potencias de dos y valores impares: la búsqueda binaria es propensa a
     * fallar justo en los límites de sus divisiones. */
    uint32_t casos[] = {1, 2, 3, 7, 8, 9, 127, 128, 129, 511, 512, 513, 999};

    for (size_t i = 0; i < ARRAY_SIZE(casos); i++) {
        zassert_equal(contar(1000, casos[i], NULL), casos[i],
                      "falla con %u registros escritos", casos[i]);
    }
}

ZTEST(record_index, test_recorre_todo_el_rango)
{
    /* Exhaustivo sobre un rango pequeño: cada posición posible. */
    for (uint32_t n = 0; n <= 64; n++) {
        zassert_equal(contar(64, n, NULL), n,
                      "falla con n=%u sobre max=64", n);
    }
}

ZTEST(record_index, test_coste_logaritmico)
{
    /* El tamaño real del proyecto: 130944 registros de 32 B en 4 MB.
     * log2(130944) ≈ 17, así que un puñado de lecturas basta. Si alguien
     * sustituyera esto por una búsqueda lineal, el arranque pasaría de
     * milisegundos a minutos y este test lo detectaría. */
    uint32_t llamadas = 0;

    zassert_equal(contar(130944, 65000, &llamadas), 65000, NULL);
    zassert_true(llamadas <= 20,
                 "esperadas ~17 lecturas, hubo %u — ¿búsqueda lineal?",
                 llamadas);
}

ZTEST(record_index, test_max_cero)
{
    zassert_equal(contar(0, 0, NULL), 0,
                  "un almacén de capacidad cero no debe romper");
}

ZTEST(record_index, test_callback_nula)
{
    zassert_equal(record_index_find_count(1000, NULL, NULL), 0,
                  "sin forma de comprobar, no se inventa un resultado");
}

ZTEST_SUITE(record_index, NULL, NULL, NULL, NULL, NULL);
