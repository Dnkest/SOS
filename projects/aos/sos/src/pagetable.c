#include "pagetable.h"
#include "vframe_table.h"
#include <sel4/sel4_arch/mapping.h>
#include "utils/kmalloc.h"
#include "vmem_layout.h"
#include <stdio.h>

struct pagetable {
    void *next_level[BIT(9)];
};

pagetable_t *pagetable_create()
{
    return (pagetable_t *)kmalloc(sizeof(pagetable_t));
}

void pagetable_destroy_impl(void *table, int *depth)
{
    pagetable_t *t = (pagetable_t *)table;
    if (*depth == 0) {
        // for (int i = 0; i < BIT(9); i++) {
        //     if (t->next_level[i] != NULL) {

        //     }
        // }
    } else {
        for (int i = 0; i < BIT(9); i++) {
            if (t->next_level[i] >= SOS_KMALLOC) {
                //printf("n:%p\n", t->next_level[i]);
                (*depth)--;
                pagetable_destroy_impl(t->next_level[i], depth);
            }
        }
    }
    //printf("%p\n", table);
    kfree(table);
}

void pagetable_destroy(pagetable_t *table)
{
    int depth = 3;
    pagetable_destroy_impl((void *)table, &depth);
}

void pagetable_put_impl(void *table, seL4_Word vaddr, vframe_ref_t vframe, int *depth)
{
    pagetable_t *t = (pagetable_t *)table;
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    if (*depth == 0) {
        t->next_level[index] = (void *)vframe;
    } else {
        if (!t->next_level[index]) {
            t->next_level[index] = (void *)pagetable_create();
        }
        (*depth)--;
        pagetable_put_impl(t->next_level[index], vaddr, vframe, depth);
    }
}

void pagetable_put(pagetable_t *table, seL4_Word vaddr, vframe_ref_t vframe)
{
    int depth = 3;
    pagetable_put_impl((void *)table, vaddr, vframe, &depth);
}

vframe_ref_t pagetable_lookup_impl(pagetable_t *table, seL4_Word vaddr, int *depth)
{
    
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    if (*depth == 0) {
        void *p = table->next_level[index];
        if (p == NULL) { return 0; }
        return (vframe_ref_t)table->next_level[index];
    } else if (!table->next_level[index]) {
        return 0;
    } else {
        (*depth)--;
        return pagetable_lookup_impl(table->next_level[index], vaddr, depth);
    }
}

vframe_ref_t pagetable_lookup(pagetable_t *table, seL4_Word vaddr)
{
    int depth = 3;
    return pagetable_lookup_impl((void *)table, vaddr, &depth);
}
