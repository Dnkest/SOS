#pragma once

#define CAPACITY 4096 - sizeof(unsigned int)

typedef struct id_table {
    unsigned char array[CAPACITY];
    unsigned int next;
} low_avail_id_t;

low_avail_id_t *id_table_init(unsigned int first);

unsigned int id_next(low_avail_id_t *table);

unsigned int id_next_start_at(low_avail_id_t *table, unsigned int index);

unsigned int id_find_n(low_avail_id_t *table, int n);

void low_avail_id_free(low_avail_id_t *table, unsigned int id);

void id_tests(low_avail_id_t *table);
