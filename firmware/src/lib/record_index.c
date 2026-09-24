#include "record_index.h"
#include <stddef.h>

uint32_t record_index_find_count(uint32_t max_records,
                                 record_is_empty_fn is_empty,
                                 void *ctx)
{
    if (max_records == 0 || is_empty == NULL) {
        return 0;
    }

    uint32_t lo = 0;
    uint32_t hi = max_records;

    /* Invariante: la frontera está en [lo, hi]. Se estrecha hasta que ambos
     * coinciden, que es el primer índice vacío. */
    while (lo < hi) {
        /* lo + (hi - lo) / 2 en vez de (lo + hi) / 2: con max_records
         * cercano a UINT32_MAX la suma desbordaría. */
        uint32_t mid = lo + (hi - lo) / 2U;

        if (is_empty(mid, ctx)) {
            hi = mid;
        } else {
            lo = mid + 1U;
        }
    }

    return lo;
}
