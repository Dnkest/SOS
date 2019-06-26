#pragma once

#include <sel4/sel4.h>

#include "frame_table.h"

addrspace_t *as_create(cspace_t *cspace, seL4_CPtr vspace);

void as_define_region(struct addrspace *as, seL4_Word vaddr, size_t memsize, char flags);

seL4_Error sos_map_frame(addrspace_t *as, cspace_t *cspace, seL4_CPtr frame_cap, seL4_CPtr vspace,
                            seL4_Word vaddr, seL4_CapRights_t rights, seL4_ARM_VMAttributes attr);
