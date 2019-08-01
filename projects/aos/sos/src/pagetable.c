#include "pagetable.h"
#include "vframe_table.h"
#include <sel4/sel4_arch/mapping.h>
#include "utils/kmalloc.h"
#include "vmem_layout.h"
#include <stdio.h>

#define CAPACITY (PAGE_SIZE_4K-sizeof(struct frame_list *)-sizeof(int))/sizeof(seL4_CPtr)

typedef struct frame_list {
    vframe_ref_t vframes[CAPACITY];
    int size;
    struct frame_list *next;
} frame_list_t;

struct pt {
    void *next_level[BIT(9)];
};

struct pagetable {
    struct pt *table;
    frame_list_t *fl;
};

frame_list_t *frame_list_create();
void frame_list_insert(frame_list_t *list, vframe_ref_t fr);
void frame_list_destroy(frame_list_t *list);

pagetable_t *pagetable_create()
{
    pagetable_t *t = (pagetable_t *)kmalloc(sizeof(pagetable_t));
    t->table = (struct pagetable *)kmalloc(sizeof(struct pt));
    t->fl = frame_list_create();
    return t;
}

void pagetable_destroy(pagetable_t *table)
{
    frame_list_destroy(table->fl);
}

void pagetable_put_impl(void *table, frame_list_t *fl, seL4_Word vaddr, vframe_ref_t vframe, int *depth)
{
    struct pt *t = (struct pt *)table;
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    if (*depth == 0) {
        t->next_level[index] = (void *)vframe;
        frame_list_insert(fl, vframe);
    } else {
        if (!t->next_level[index]) {
            t->next_level[index] = (void *)kmalloc(sizeof(struct pt));
        }
        (*depth)--;
        pagetable_put_impl(t->next_level[index], fl, vaddr, vframe, depth);
    }
}

void pagetable_put(pagetable_t *table, seL4_Word vaddr, vframe_ref_t vframe)
{
    int depth = 3;
    pagetable_put_impl((void *)table->table, table->fl, vaddr, vframe, &depth);
}

vframe_ref_t pagetable_lookup_impl(void *table, seL4_Word vaddr, int *depth)
{
    int shift_amount = 12 + (*depth) * 9;
    int index = (vaddr >> shift_amount) & 0b111111111;
    if (*depth == 0) {
        void *p = ((struct pt *)table)->next_level[index];
        if (p == NULL) { return 0; }
        return (vframe_ref_t)((struct pt *)table)->next_level[index];
    } else if (!((struct pt *)table)->next_level[index]) {
        return 0;
    } else {
        (*depth)--;
        return pagetable_lookup_impl(((struct pt *)table)->next_level[index], vaddr, depth);
    }
}

vframe_ref_t pagetable_lookup(pagetable_t *table, seL4_Word vaddr)
{
    int depth = 3;
    return pagetable_lookup_impl((void *)table->table, vaddr, &depth);
}

frame_list_t *frame_list_create()
{
    frame_list_t *list = (frame_list_t *)kmalloc(sizeof(frame_list_t));
    list->size = 0;
    list->next = NULL;
    return list;
}

void frame_list_insert(frame_list_t *list, vframe_ref_t fr)
{
    while (list != NULL && list->next != NULL) { list = list->next; }
    if (list->size == CAPACITY) {
        list->next = frame_list_create();
        list = list->next;
    }
    list->vframes[list->size++] = fr;
}

void frame_list_destroy(frame_list_t *list)
{
    frame_list_t *t;
    while (list != NULL) {
        for (int i = 0; i < list->size; i++) {
            free_vframe(list->vframes[i]);
        }
        t = list;
        list = list->next;
        kfree(t);
    }
}
