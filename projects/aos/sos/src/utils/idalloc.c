#include "idalloc.h"
#include "kmalloc.h"

struct id_table {
    void *base;
    unsigned int capacity;
    char *bitmap;
    unsigned int unit;
};

id_table_t *id_table_init(void *base, unsigned int unit, unsigned int capacity)
{
    id_table_t *table = (id_table_t *)kmalloc(sizeof(id_table_t));
    table->base = base;
    table->capacity = capacity;
    table->bitmap = (char *)kmalloc(capacity *sizeof(char));
    table->unit = unit;
    return table;
}

void *id_alloc(id_table_t *table, unsigned int n)
{
    int i = 0;
    while (1) {
        if (i + n > table->capacity) { return -1; }
        
        if (!table->bitmap[i]) {
            int j;
            for (j = 0; j < n; j++) {
                if (table->bitmap[i+j]) {
                    i += (j + 1);
                    break;
                }
            }
            if (j == n) {
                for (j = 0; j < n; j++) { table->bitmap[i+j] = 1; }
                return table->base + i * table->unit;
            }
        } else {
            i++;
        }
    }
}

void id_free(id_table_t *table, void *start, unsigned int n)
{
    int s = (start - table->base)/table->unit;
    for (int i = 0; i < n; i++) {
        table->bitmap[s+i] = 0;
    }
}

void id_alloc_tests()
{
    id_table_t *table = id_table_init(0x8000, 1 << 12, 20);
    int i = (int)id_alloc(table, 1);
    printf("i = %p\n", i);
    i = (int)id_alloc(table, 3);
    printf("i = %p\n", i);
    i = (int)id_alloc(table, 1);
    printf("i = %p\n", i);
    id_free(table, 0x9000, 3);
    i = (int)id_alloc(table, 1);
    printf("i = %p\n", i);
    i = (int)id_alloc(table, 2);
    printf("i = %p\n", i);
}
