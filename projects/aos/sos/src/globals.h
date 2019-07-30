#pragma once

#include <cspace/cspace.h>
#include <sel4/sel4.h>

void set_global_cspace(cspace_t *cspace);
void set_global_vspace(seL4_CPtr vspace);
void set_ipc_ep(seL4_CPtr ep);

cspace_t *global_cspace();
seL4_CPtr global_vspace();
seL4_CPtr ipc_ep();
