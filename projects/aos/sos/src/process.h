#pragma once

#include <cspace/cspace.h>
#include "addrspace.h"
#include "ut.h"
#include "fs/fd_table.h"

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
    fd_table_t *fdt;

    seL4_Word sp;

    seL4_CPtr reply;
} process_t;

typedef struct proc_map {
    unsigned int num_pages;
    seL4_CPtr *frame_caps;
    seL4_Word vaddr;
} proc_map_t;

void set_cur_proc(process_t *proc);
process_t *cur_proc();

process_t *process_create(cspace_t *cspace, char *cpio_archive, char *cpio_archive_end);
void process_start(process_t *proc, char *app_name, addrspace_t *global_addrspace, seL4_CPtr ep);

seL4_Word process_map(process_t *proc, seL4_Word user_vaddr, seL4_Word size,
                        addrspace_t *global_addrspace, seL4_CPtr global_vspace,
                        proc_map_t *mapped);
void process_unmap(process_t *proc, proc_map_t *mapped);
