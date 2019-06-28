#pragma once

#include <cspace/cspace.h>
#include "addrspace.h"
#include "ut.h"

typedef struct pcb {
    ut_t *tcb_ut;
    seL4_CPtr tcb;
    ut_t *vspace_ut;
    seL4_CPtr vspace;

    ut_t *ipc_buffer_ut;
    seL4_CPtr ipc_buffer;

    cspace_t cspace;
    cspace_t *global_cspace;

    ut_t *stack_ut;
    seL4_CPtr stack;

    addrspace_t *as;

    seL4_CPtr reply;
} pcb_t;

void set_cur_proc(pcb_t *proc);
pcb_t *get_cur_proc();
