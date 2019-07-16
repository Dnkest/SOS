#pragma once

typedef struct id_table id_table_t;

id_table_t *id_table_init(void *base, unsigned int unit, unsigned int capacity);
void *id_alloc(id_table_t *table, unsigned int n);
void id_free(id_table_t *table, void *start, unsigned int n);
void id_alloc_tests();
