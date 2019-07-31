
#pragma once

#include <sel4/sel4.h>
#include <stdbool.h>
#include <stdlib.h>

#include "pagetable.h"
#include "ut.h"

typedef struct cap_list cap_list_t;

typedef struct region {
    seL4_Word base;
    seL4_Word top;
    bool read;
    bool write;

    struct as_region *next;
} region_t;

typedef struct addrspace {
    region_t *regions;
    pagetable_t *table;
    unsigned int pages;
    cap_list_t *cap_list;
} addrspace_t;

addrspace_t *addrspace_create();

frame_ref_t addrspace_lookup_vframe(addrspace_t *addrspace, seL4_Word vaddr);
int addrspace_vaddr_exists(addrspace_t *addrspace, seL4_Word vaddr);

bool addrspace_check_valid_region(addrspace_t *addrspace, seL4_Word fault_address);

void addrspace_define_region(addrspace_t *addrspace, seL4_Word vaddr,
                                size_t memsize, unsigned long permissions);

seL4_Error addrspace_alloc_map_one_page(addrspace_t *addrspace, cspace_t *cspace, seL4_CPtr frame_cap,
                                    seL4_CPtr vspace, seL4_Word vaddr);
void addrspace_destory(addrspace_t *addrspace);
