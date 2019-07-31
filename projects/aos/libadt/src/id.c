#include <adt/id.h>
#include <stdlib.h>
#include <assert.h>

low_avail_id_t *low_avail_id_init(unsigned int first)
{
    low_avail_id_t *table = malloc(sizeof(struct id_table));
    table->next = 0;
    for (int i = 0; i< 4092; i++) {
        table->array[i] = 0;
    }
    return table;
}

unsigned int id_next(low_avail_id_t *table)
{

    unsigned int ret = table->next;
    table->array[ret] = 1;
    int tmp = ret + 1;
    while (table->array[tmp]) {
        tmp++;
    }
    table->next = tmp;
    return ret;
}

unsigned int id_next_start_at(low_avail_id_t *table, unsigned int index)
{
    unsigned int tmp = table->next;
    table->next = index;
    unsigned int ret = id_next(table);
    table->next = tmp;
    return ret;
}

unsigned int id_find_n(low_avail_id_t *table, int n)
{
    int ret = table->next;
    int tmp = ret;
    int tmp_n = n;
    while (1) {
        if (tmp_n == 0) {
            return ret;
        } else if (table->array[tmp] == 0) {
            tmp_n--;
            tmp++;
        } else {
            tmp_n = n;
            tmp++;
            ret = tmp;
        }
    }
}

void low_avail_id_free(low_avail_id_t *table, unsigned int id)
{
    table->array[id] = 0;
    int tmp = 0;
    while (table->array[tmp]) {
        tmp++;
    }
    table->next = (unsigned int)tmp;
}

void id_tests(low_avail_id_t *table)
{
    low_avail_id_t *table1 = low_avail_id_init(0);

    unsigned int t1 = id_next(table1);
    assert(t1 == 0);
    unsigned int t2 = id_next(table1);
    assert(t2 == 1);
    unsigned int t3 = id_next(table1);
    assert(t3 == 2);
    low_avail_id_free(table1, 0);
    unsigned int t4 = id_next(table1);
    assert(t4 == 0);
    unsigned int t5 = id_find_n(table1, 3);
    assert(t5 == 3);
    unsigned int t6 = id_next_start_at(table1, 3);
    assert(t6 == 3);
}