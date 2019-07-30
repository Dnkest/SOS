#pragma once

#include <cspace/cspace.h>

#include "addrspace.h"
#include "proc.h"

#define SYSCALL_MAX             14
#define SOS_SYSCALL_OPEN        1
#define SOS_SYSCALL_WRITE       2
#define SOS_SYSCALL_READ        3
#define SOS_SYSCALL_BRK         4
#define SOS_SYSCALL_GETDIRENT   5
#define SOS_SYSCALL_STAT        6
#define SOS_SYSCALL_CLOSE       7
#define SOS_SYSCALL_PROC_CREATE 8

void syscall_handlers_init();
void *sos_handle_syscall(void *data);
void *sos_handle_vm_fault(void *data);
