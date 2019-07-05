#pragma once

#include <cspace/cspace.h>
#include "addrspace.h"
#include "ut.h"

typedef struct process {
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

    char *cpio_archive;
    char *cpio_archive_end;

    addrspace_t *addrspace;

    seL4_Word sp;

    seL4_CPtr reply;
} process_t;

void proc_set(process_t *proc);
process_t *proc_get();

process_t *process_create(cspace_t *cspace, char *cpio_archive, char *cpio_archive_end);
void process_start(process_t *proc, char *app_name, addrspace_t *global_addrspace, seL4_CPtr ep);

seL4_Word process_write(process_t *proc, addrspace_t *global_addrspace, seL4_CPtr vspace,
                                seL4_Word user_vaddr, seL4_Word size);

seL4_Word process_read(process_t *proc, addrspace_t *global_addrspace, seL4_CPtr vspace,
                                seL4_Word user_vaddr, seL4_Word size);
