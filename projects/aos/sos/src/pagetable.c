#include "pagetable.h"
#include "frame_table.h"
#include <sel4/sel4_arch/mapping.h>
#include "utils/kmalloc.h"

typedef struct leaf {
    vframe_ref_t frame;
    seL4_Word frame_cap;
} leaf_t;

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

void pagetable_put_impl(void *table, seL4_Word vaddr, vframe_ref_t paddr, seL4_Word frame_cap, int *depth)
{
    pagetable_t *t = (pagetable_t *)table;
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    //printf("p(%d) ", index);
    if (*depth == 0) {
        //printf("paddr %p\n", paddr);
        leaf_t *l = (leaf_t *)kmalloc(sizeof(leaf_t));
        l->frame = paddr;
        l->frame_cap = frame_cap;
        t->next_level[index] = (void *)l;
    } else {

        if (!t->next_level[index]) {
            // if (*depth == 1) {
            //     t->next_level[index] = (void *)pageleaf_create();
            // } else {
                t->next_level[index] = (void *)pagetable_create();
            //}
        }
        (*depth)--;
        pagetable_put_impl(t->next_level[index], vaddr, paddr, frame_cap, depth);
    }
}

void pagetable_put(pagetable_t *table, seL4_Word vaddr, vframe_ref_t paddr, seL4_Word frame_cap)
{
    int depth = 3;
    pagetable_put_impl((void *)table, vaddr, paddr, frame_cap, &depth);
    //printf("\n");
}

void *pagetable_lookup_impl(pagetable_t *table, seL4_Word vaddr, int *depth, int flag)
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
        leaf_t *l = (leaf_t *)table->next_level[index];
        if (flag == 1) {
            return (void *)l->frame;
        } else {
            return (void *)l->frame_cap;
        }
        
    } else if (!table->next_level[index]) {
        //printf("\n");
        return NULL;
    } else {
        (*depth)--;
        return pagetable_lookup_impl(table->next_level[index], vaddr, depth, flag);
    }
}

vframe_ref_t pagetable_lookup_vframe(pagetable_t *table, seL4_Word vaddr)
{
    int depth = 3;
    return (vframe_ref_t)pagetable_lookup_impl((void *)table, vaddr, &depth, 1);
}

seL4_Word pagetable_lookup_frame_cap(pagetable_t *table, seL4_Word vaddr)
{
    int depth = 3;
    return (seL4_Word)pagetable_lookup_impl((void *)table, vaddr, &depth, 0);
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

void pagetable_unmap_all_impl(void *table, int depth)
{
    pagetable_t *t = (pagetable_t *)table;
    for (int i = 0; i < 512; i++) {
        if (depth == 0) {
            vframe_ref_t frame = *((vframe_ref_t *)t->next_level[i]);
            if (frame != 0) {
                seL4_Error err = seL4_ARM_Page_Unmap(frame_page(frame));
                assert(err == seL4_NoError);

                /* delete the copy of the stack frame cap */
                err = cspace_delete(frame_table_cspace(), frame_page(frame));
                assert(err == seL4_NoError);

                /* mark the slot as free */
                cspace_free_slot(frame_table_cspace(), frame_page(frame));
                kfree(t->next_level[i]);
                t->next_level[i] = NULL;
            }
        } else if (!t->next_level[i]) {
            pagetable_unmap_all_impl(t->next_level[i], depth - 1);
        }
    }
}

void pagetable_unmap_all(pagetable_t *table)
{
    return pagetable_unmap_all_impl((void *)table, 3);
}
