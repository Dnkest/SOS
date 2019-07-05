#include "pagetable.h"
#include "frame_table.h"
#include <sel4/sel4_arch/mapping.h>

typedef struct leaf {
    frame_ref_t frames[BIT(9)];
} leaf_t;

struct pagetable {
    void *next_level[BIT(9)];
};

pagetable_t *pagetable_create()
{
    return (pagetable_t *)alloc_one_page();
}

leaf_t *pageleaf_create()
{
    return (leaf_t *)alloc_one_page();
}

void pagetable_put_impl(void *table, seL4_Word vaddr, frame_ref_t paddr, int *depth)
{
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    if (*depth == 0) {
        leaf_t *leaf = (leaf_t *)table;
        leaf->frames[index] = paddr;
    } else {
        pagetable_t *t = (pagetable_t *)table;
        if (!t->next_level[index]) {
            if (*depth == 1) {
                t->next_level[index] = (void *)pageleaf_create();
            } else {
                t->next_level[index] = (void *)pagetable_create();
            }
        }
        (*depth)--;
        pagetable_put_impl(t->next_level[index], vaddr, paddr, depth);
    }
}

void pagetable_put(pagetable_t *table, seL4_Word vaddr, frame_ref_t paddr)
{
    int depth = 3;
    pagetable_put_impl((void *)table, vaddr, paddr, &depth);
}

frame_ref_t pagetable_lookup_impl(pagetable_t *table, seL4_Word vaddr, int *depth)
{
    
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    if (*depth == 0) {
        leaf_t *leaf = (leaf_t *)table;
        return leaf->frames[index];
    } else if (!table->next_level[index]) {
        return 0;
    } else {
        (*depth)--;
        return pagetable_lookup_impl(table->next_level[index], vaddr, depth);
    }
}

frame_ref_t pagetable_lookup(pagetable_t *table, seL4_Word vaddr)
{
    int depth = 3;
    return pagetable_lookup_impl((void *)table, vaddr, &depth);
}
