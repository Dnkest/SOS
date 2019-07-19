#pragma once

#include <cspace/cspace.h>

#include "addrspace.h"
#include "process.h"

#define SYSCALL_MAX             10
#define SOS_SYSCALL_OPEN        1
#define SOS_SYSCALL_WRITE       2
#define SOS_SYSCALL_READ        3
#define SOS_SYSCALL_BRK         4
#define SOS_SYSCALL_GETDIRENT   5
#define SOS_SYSCALL_STAT        6
#define SOS_SYSCALL_CLOSE       7

void syscall_handler_init(cspace_t *cspace, seL4_CPtr vspace, addrspace_t *addrspace);
void sos_handle_syscall(process_t *proc);
bool sos_handle_page_fault(process_t *proc, seL4_Word fault_address);

void do_jobs();

cspace_t *get_global_cspace();
seL4_CPtr get_global_vspace();
addrspace_t *get_global_addrspace();
