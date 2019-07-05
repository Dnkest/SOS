#pragma once

#define CAPACITY 4096 - sizeof(unsigned int)

typedef struct id_table {
    unsigned char array[CAPACITY];
    unsigned int next;
} id_table_t;

id_table_t *id_table_init(unsigned int first);

unsigned int id_next(id_table_t *table);

unsigned int id_next_start_at(id_table_t *table, unsigned int index);

unsigned int id_find_n(id_table_t *table, int n);

void id_free(id_table_t *table, unsigned int id);

void id_tests(id_table_t *table);
