#include "low_avail_id.h"
#include "kmalloc.h"

struct low_avail_id {
    void *base;
    unsigned int capacity;
    char *bitmap;
    unsigned int unit;
};

low_avail_id_t *low_avail_id_init(void *base, unsigned int unit, unsigned int capacity)
{
    low_avail_id_t *table = (low_avail_id_t *)kmalloc(sizeof(low_avail_id_t));
    table->base = base;
    table->capacity = capacity;
    table->bitmap = (char *)kmalloc(capacity *sizeof(char));
    table->unit = unit;
    return table;
}

void *low_avail_id_alloc(low_avail_id_t *table, unsigned int n)
{
    unsigned int i = 0;
    while (1) {
        if (i + n > table->capacity) { return -1; }
        
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
                return table->base + i * table->unit;
            }
        } else {
            i++;
        }
    }
    return NULL;
}

void low_avail_id_free(low_avail_id_t *table, void *start, unsigned int n)
{
    unsigned int s = (start - table->base)/table->unit;
    for (unsigned int i = 0; i < n; i++) {
        table->bitmap[s+i] = 0;
    }
}

void low_avail_id_destroy(low_avail_id_t *table)
{
    kfree(table->bitmap);
    kfree(table);
}

void id_alloc_tests()
{
    low_avail_id_t *table = low_avail_id_init(0x8000, 1 << 12, 20);
    int i = (int)low_avail_id_alloc(table, 1);
    printf("i = %p\n", i);
    i = (int)low_avail_id_alloc(table, 3);
    printf("i = %p\n", i);
    i = (int)low_avail_id_alloc(table, 1);
    printf("i = %p\n", i);
    low_avail_id_free(table, 0x9000, 3);
    i = (int)low_avail_id_alloc(table, 1);
    printf("i = %p\n", i);
    i = (int)low_avail_id_alloc(table, 2);
    printf("i = %p\n", i);
}
