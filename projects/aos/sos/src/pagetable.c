#include "pagetable.h"
#include "frame_table.h"
#include <sel4/sel4_arch/mapping.h>
#include "utils/kmalloc.h"

// typedef struct leaf {
//     frame_ref_t frames[BIT(9)];
// } leaf_t;

struct pagetable {
    void *next_level[BIT(9)];
};

pagetable_t *pagetable_create()
{
    return (pagetable_t *)kmalloc(sizeof(pagetable_t));
}

// leaf_t *pageleaf_create()
// {
//     return (leaf_t *)kmalloc(sizeof(leaf_t));
// }

void pagetable_put_impl(void *table, seL4_Word vaddr, frame_ref_t paddr, int *depth)
{
    pagetable_t *t = (pagetable_t *)table;
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    //printf("p(%d) ", index);
    if (*depth == 0) {
        //printf("paddr %p\n", paddr);
        frame_ref_t *f = (frame_ref_t *)kmalloc(sizeof(frame_ref_t));
        *f = paddr;
        t->next_level[index] = (void *)f;
    } else {

        if (!t->next_level[index]) {
            // if (*depth == 1) {
            //     t->next_level[index] = (void *)pageleaf_create();
            // } else {
                t->next_level[index] = (void *)pagetable_create();
            //}
        }
        (*depth)--;
        pagetable_put_impl(t->next_level[index], vaddr, paddr, depth);
    }
}

void pagetable_put(pagetable_t *table, seL4_Word vaddr, frame_ref_t paddr)
{
    int depth = 3;
    pagetable_put_impl((void *)table, vaddr, paddr, &depth);
    //printf("\n");
}

frame_ref_t pagetable_lookup_impl(pagetable_t *table, seL4_Word vaddr, int *depth)
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
        return *((frame_ref_t *)table->next_level[index]);
    } else if (!table->next_level[index]) {
        //printf("\n");
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
