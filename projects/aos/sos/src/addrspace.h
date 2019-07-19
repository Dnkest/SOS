
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
    //as_page_table_t *as_page_table;
    region_t *regions;
    pagetable_t *table;
    ut_t *untypeds;
    cap_list_t *cap_list;
} addrspace_t;

addrspace_t *addrspace_create();

frame_ref_t addrspace_lookup(addrspace_t *addrspace, seL4_Word vaddr);

bool addrspace_check_valid_region(addrspace_t *addrspace, seL4_Word fault_address);

void addrspace_define_region(addrspace_t *addrspace, seL4_Word vaddr,
                                size_t memsize, unsigned long permissions);

seL4_Error addrspace_alloc_map_one_page(addrspace_t *addrspace, cspace_t *cspace, seL4_CPtr frame_cap,
                                    seL4_CPtr vspace, seL4_Word vaddr);

seL4_Error addrspace_map_one_page(addrspace_t *target_addrspace, cspace_t *target_cspace,
                                    seL4_CPtr target, seL4_CPtr vspace, seL4_Word target_vaddr,
                                    addrspace_t *source_addrspace, cspace_t *source_cspace,
                                    seL4_Word source_vaddr);
seL4_Error addrspace_map_one_frame(addrspace_t *target_addrspace, cspace_t *target_cspace,
                                    seL4_CPtr target, seL4_CPtr vspace, seL4_Word target_vaddr,
                                    frame_ref_t frame);
seL4_Error addrspace_unmap(addrspace_t *addrspace, cspace_t *cspace,
                            seL4_CPtr frame_cap, seL4_Word vaddr);