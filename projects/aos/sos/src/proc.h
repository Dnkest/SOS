#pragma once

#include <stdbool.h>

#include "addrspace.h"
#include "fs/fd_table.h"

typedef struct proc proc_t;

int process_init(char *app_name, seL4_CPtr ep);

void process_set_exiting(int pid);

int process_id_exits(int pid);

int process_exiting(proc_t *proc);

int process_any_exits();

void process_delete(proc_t *proc);

int process_max();

int process_exists_by_badge(seL4_Word badge);

proc_t *process_get_by_badge(seL4_Word badge);

int process_exists_by_id(int pid);

proc_t *process_get_by_id(int pid);

void process_child_add_parent(proc_t *proc, int pid);

int process_not_killing_parent(proc_t *proc, int pid);

void process_reply(proc_t *proc, unsigned int msg_len);

seL4_CPtr process_get_reply_cap(proc_t *proc);

void process_set_reply_cap(proc_t *proc, seL4_CPtr reply);

char *process_name();

seL4_CPtr process_tcb(proc_t *proc);

fd_table_t *process_fdt(proc_t *proc);

addrspace_t *process_addrspace(proc_t *proc);

seL4_CPtr process_vspace(proc_t *proc);

int process_id(proc_t *proc);

unsigned process_time(proc_t *proc);

unsigned process_size(proc_t *proc);

seL4_Word process_get_data0(proc_t *proc);
seL4_Word process_get_data1(proc_t *proc);
seL4_Word process_get_data2(proc_t *proc);
seL4_Word process_get_data3(proc_t *proc);
void process_set_data0(proc_t *proc, seL4_Word data);
void process_set_data1(proc_t *proc, seL4_Word data);
void process_set_data2(proc_t *proc, seL4_Word data);
void process_set_data3(proc_t *proc, seL4_Word data);
