#include "circular_id.h"
#include "kmalloc.h"
#include <stdio.h>

struct circular_id {
    void *base;
    unsigned int capacity;
    unsigned int ptr;
    char *bitmap;
    unsigned int unit;
};

circular_id_t *circular_id_init(void *base, unsigned int unit, unsigned int capacity)
{
    circular_id_t *table = (circular_id_t *)kmalloc(sizeof(circular_id_t));
    table->base = base;
    table->capacity = capacity;
    table->ptr = 0;
    table->bitmap = (char *)kmalloc(capacity *sizeof(char));
    table->unit = unit;
    return table;
}

void *circular_id_alloc(circular_id_t *table, unsigned int n)
{
    unsigned int i = table->ptr;
    int wrapped = 0;
    while (!wrapped) {
        if (i + n >= table->capacity) {
            wrapped = 1;
            i = table->ptr = 0;
        }
        
        if (!table->bitmap[i]) {
            int f = 0;
            for (unsigned int j = 0; j < n; j++) {
                if (table->bitmap[i+j]) {
                    i += (j + 1);
                    f = 1;
                }
            }
            if (!f) {
                unsigned int j;
                for (j = 0; j < n; j++) { table->bitmap[i+j] = 1; }
                table->ptr = i+j;
                return table->base + i * table->unit;
            }
        } else {
            i++;
        }
    }
    return NULL;
}

void circular_id_free(circular_id_t *table, void *start, unsigned int n)
{
    unsigned int s = (start - table->base)/table->unit;
    for (unsigned int i = 0; i < n; i++) {
        table->bitmap[s+i] = 0;
    }
}

void circular_id_destroy(circular_id_t *table)
{
    kfree(table->bitmap);
    kfree(table);
}
