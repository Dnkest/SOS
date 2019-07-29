#pragma once

#include <stdbool.h>

#include "addrspace.h"
#include "fs/fd_table.h"

typedef struct proc proc_t;

bool process_init(char *app_name, seL4_CPtr ep);
proc_t *process_get_by_badge(seL4_Word badge);

seL4_CPtr process_get_reply_cap(proc_t *proc);
void process_reply(proc_t *proc, unsigned int msg_len);
void process_set_reply_cap(proc_t *proc, seL4_CPtr reply);
const char *process_get_name();
seL4_CPtr process_get_tcb(proc_t *proc);
fd_table_t *process_fdt(proc_t *proc);
addrspace_t *process_addrspace(proc_t *proc);
seL4_CPtr process_vspace(proc_t *proc);

seL4_Word process_get_data0(proc_t *proc);
seL4_Word process_get_data1(proc_t *proc);
seL4_Word process_get_data2(proc_t *proc);
seL4_Word process_get_data3(proc_t *proc);
void process_set_data0(proc_t *proc, seL4_Word data);
void process_set_data1(proc_t *proc, seL4_Word data);
void process_set_data2(proc_t *proc, seL4_Word data);
void process_set_data3(proc_t *proc, seL4_Word data);
