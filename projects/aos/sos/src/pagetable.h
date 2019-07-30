#pragma once

#include <sel4/sel4.h>

#include "vframe_table.h"

typedef struct pagetable pagetable_t;

pagetable_t *pagetable_create();
void pagetable_put(pagetable_t *table, seL4_Word vaddr, vframe_ref_t vframe);
vframe_ref_t pagetable_lookup(pagetable_t *table, seL4_Word vaddr);
