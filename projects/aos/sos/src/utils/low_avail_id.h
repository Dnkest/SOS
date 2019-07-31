#pragma once

typedef struct low_avail_id low_avail_id_t;

low_avail_id_t *low_avail_id_init(void *base, unsigned int unit, unsigned int capacity);
void *low_avail_id_alloc(low_avail_id_t *table, unsigned int n);
void low_avail_id_free(low_avail_id_t *table, void *start, unsigned int n);
void low_avail_id_destroy(low_avail_id_t *table);
