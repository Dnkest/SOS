#pragma once

#include <cspace/cspace.h>

#include "addrspace.h"
#include "process.h"

void syscall_handler_init(cspace_t *cspace, seL4_CPtr vspace, addrspace_t *addrspace);
void sos_handle_syscall(process_t *proc);
bool sos_handle_page_fault(process_t *proc, seL4_Word fault_address);

void do_jobs();