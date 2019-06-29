#pragma once

#include <sel4/sel4.h>
#include <stdbool.h>

#include "pagetable.h"

typedef struct as_region {
    seL4_Word base;
    seL4_Word top;
    bool read;
    bool write;

    struct as_region *next;
} as_region_t;

typedef struct addrspace {
    as_page_table_t *as_page_table;
    as_region_t *regions;
} addrspace_t;

uintptr_t as_alloc_one_page();
void as_free(uintptr_t vaddr);

addrspace_t *as_create();

void as_define_region(addrspace_t *as, seL4_Word vaddr, size_t memsize, unsigned long permissions);

bool sos_handle_page_fault(seL4_Word fault_address);
