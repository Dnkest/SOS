#include <adt/circular_id.h>
#include <stdlib.h>

struct ccircular_id {
    void *base;
    unsigned int capacity;
    unsigned int ptr;
    char *bitmap;
    unsigned int unit;
};

ccircular_id_t *ccircular_id_init(void *base, unsigned int unit, unsigned int capacity)
{
    ccircular_id_t *table = (ccircular_id_t *)malloc(sizeof(ccircular_id_t));
    table->base = base;
    table->capacity = capacity;
    table->ptr = 0;
    table->bitmap = (char *)malloc(capacity *sizeof(char));
    for (int i = 0; i < table->capacity; i++) { table->bitmap[i] = 0; }
    table->unit = unit;
    return table;
}

void *ccircular_id_alloc(ccircular_id_t *table, unsigned int n)
{
    unsigned int i = table->ptr;
    int wrapped = 0;
    while (!wrapped) {
        if (i + n >= table->capacity) {
            wrapped = 1;
            i = table->ptr = 0;
        }
        
        if (!table->bitmap[i]) {
            unsigned int j;
            for (j = 0; j < n; j++) {
                if (table->bitmap[i+j]) {
                    i += (j + 1);
                    break;
                }
            }
            if (j == n) {
                for (j = 0; j < n; j++) { table->bitmap[i+j] = 1; }
                table->ptr = i+j;
                return table->base + i * table->unit;
            }
        } else {
            i++;
        }
    }
    return -1;
}

void ccircular_id_free(ccircular_id_t *table, void *start, unsigned int n)
{
    unsigned int s = (start - table->base)/table->unit;
    for (unsigned int i = 0; i < n; i++) {
        table->bitmap[s+i] = 0;
    }
}

void ccircular_id_destroy(ccircular_id_t *table)
{
    free(table->bitmap);
    free(table);
}
