#pragma once

typedef struct circular_id circular_id_t;

circular_id_t *circular_id_init(void *base, unsigned int unit, unsigned int capacity);
void *circular_id_alloc(circular_id_t *table, unsigned int n);
void circular_id_free(circular_id_t *table, void *start, unsigned int n);
void circular_id_destroy(circular_id_t *table);
