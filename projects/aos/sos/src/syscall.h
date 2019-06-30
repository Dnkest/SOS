#pragma once

#include <cspace/cspace.h>

#include "pagetable.h"

void syscall_handler_init(cspace_t *cspace, seL4_CPtr vspace, as_page_table_t *pagetable);
void sos_handle_syscall();

void do_jobs();