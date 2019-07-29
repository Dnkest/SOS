#pragma once

#include <cspace/cspace.h>
#include <sel4/sel4.h>

void set_global_cspace(cspace_t *cspace);
void set_global_vspace(seL4_CPtr vspace);

cspace_t *global_cspace();
seL4_CPtr global_vspace();
