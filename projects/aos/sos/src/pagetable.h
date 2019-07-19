#pragma once

#include <sel4/sel4.h>

#include "frame_table.h"

typedef struct pagetable pagetable_t;

pagetable_t *pagetable_create();
void pagetable_put(pagetable_t *table, seL4_Word vaddr, frame_ref_t paddr);
frame_ref_t pagetable_lookup(pagetable_t *table, seL4_Word vaddr);
void pagetable_delete(pagetable_t *table, seL4_Word vaddr);
