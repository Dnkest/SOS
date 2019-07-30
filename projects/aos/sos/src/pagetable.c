#include "pagetable.h"
#include "vframe_table.h"
#include <sel4/sel4_arch/mapping.h>
#include "utils/kmalloc.h"

struct pagetable {
    void *next_level[BIT(9)];
};

pagetable_t *pagetable_create()
{
    return (pagetable_t *)kmalloc(sizeof(pagetable_t));
}

void pagetable_put_impl(void *table, seL4_Word vaddr, vframe_ref_t vframe, int *depth)
{
    pagetable_t *t = (pagetable_t *)table;
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    //printf("p(%d) ", index);
    if (*depth == 0) {
        //printf("vframe %p\n", vframe);
        // vframe_ref_t *f = (vframe_ref_t *)kmalloc(sizeof(vframe_ref_t));
        // *f = vframe;
        t->next_level[index] = (void *)vframe;
    } else {

        if (!t->next_level[index]) {
            // if (*depth == 1) {
            //     t->next_level[index] = (void *)pageleaf_create();
            // } else {
                t->next_level[index] = (void *)pagetable_create();
            //}
        }
        (*depth)--;
        pagetable_put_impl(t->next_level[index], vaddr, vframe, depth);
    }
}

void pagetable_put(pagetable_t *table, seL4_Word vaddr, vframe_ref_t vframe)
{
    int depth = 3;
    pagetable_put_impl((void *)table, vaddr, vframe, &depth);
    //printf("\n");
}

vframe_ref_t pagetable_lookup_impl(pagetable_t *table, seL4_Word vaddr, int *depth)
{
    
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    //printf("l(%d) ", index);
    if (*depth == 0) {
        void *p = table->next_level[index];
        if (p == NULL) { return 0; }
        //leaf_t *leaf = (leaf_t *)table;
        //return leaf->frames[index];
        //printf("\n");
        return (vframe_ref_t)table->next_level[index];
    } else if (!table->next_level[index]) {
        //printf("\n");
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

void pagetable_del_impl(void *table, seL4_Word vaddr, int *depth)
{
    pagetable_t *t = (pagetable_t *)table;
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    if (*depth == 0) {
        kfree(t->next_level[index]);
        t->next_level[index] = NULL;
    } else {
        if (!t->next_level[index]) {
            t->next_level[index] = (void *)pagetable_create();
        }
        (*depth)--;
        pagetable_del_impl(t->next_level[index], vaddr, depth);
    }
}

void pagetable_delete(pagetable_t *table, seL4_Word vaddr)
{
    int depth = 3;
    //printf("deleting %p\n", vaddr);
    return pagetable_del_impl((void *)table, vaddr, &depth);
}
