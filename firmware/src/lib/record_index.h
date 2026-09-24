#ifndef RECORD_INDEX_H
#define RECORD_INDEX_H

#include <stdint.h>
#include <stdbool.h>

/* Localiza la frontera entre registros escritos y vacíos en un almacén que
 * se llena de forma estrictamente secuencial.
 *
 * Sirve para derivar el número de registros guardados en vez de almacenarlo:
 * un contador persistido obliga a reescribirlo en cada registro, y en una
 * flash NOR eso significa borrar su sector completo cada vez (ver
 * docs/sdd/ciclos/001-desgaste-metadatos/).
 *
 * Sin hardware ni dependencias de Zephyr — testeable en host. */

/* Devuelve true si el registro `index` está vacío (nunca escrito).
 * `ctx` transporta el estado que necesite quien la implemente. */
typedef bool (*record_is_empty_fn)(uint32_t index, void *ctx);

/* Número de registros escritos, que es también el índice del primer hueco
 * libre. Coste: log2(max_records) llamadas a `is_empty`.
 *
 * PRECONDICIÓN: monotonía. Todos los registros anteriores a la frontera
 * escritos, todos los posteriores vacíos. Si el almacén se rellena de forma
 * no secuencial, el resultado no tiene sentido.
 *
 * Casos límite: 0 si el primero está vacío; `max_records` si están todos
 * escritos. Con `max_records == 0` devuelve 0. */
uint32_t record_index_find_count(uint32_t max_records,
                                 record_is_empty_fn is_empty,
                                 void *ctx);

#endif
