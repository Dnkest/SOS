#pragma once

typedef struct ccircular_id ccircular_id_t;

ccircular_id_t *ccircular_id_init(void *base, unsigned int unit, unsigned int capacity);
void *ccircular_id_alloc(ccircular_id_t *table, unsigned int n);
void ccircular_id_free(ccircular_id_t *table, void *start, unsigned int n);
void ccircular_id_destroy(ccircular_id_t *table);
