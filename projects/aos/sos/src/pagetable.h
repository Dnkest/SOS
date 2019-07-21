#pragma once

#include <sel4/sel4.h>

#include "vft.h"

typedef struct pagetable pagetable_t;

pagetable_t *pagetable_create();
void pagetable_put(pagetable_t *table, seL4_Word vaddr, vframe_ref_t paddr, seL4_Word frame_cap);
vframe_ref_t pagetable_lookup_vframe(pagetable_t *table, seL4_Word vaddr);
seL4_Word pagetable_lookup_frame_cap(pagetable_t *table, seL4_Word vaddr);
void pagetable_delete(pagetable_t *table, seL4_Word vaddr);
void pagetable_unmap_all(pagetable_t *table);
