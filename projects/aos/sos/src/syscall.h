#pragma once

#include <cspace/cspace.h>

#include "addrspace.h"
#include "proc.h"

#define SYSCALL_MAX             20
#define SOS_SYSCALL_OPEN        1
#define SOS_SYSCALL_WRITE       2
#define SOS_SYSCALL_READ        3
#define SOS_SYSCALL_BRK         4
#define SOS_SYSCALL_GETDIRENT   5
#define SOS_SYSCALL_STAT        6
#define SOS_SYSCALL_CLOSE       7
#define SOS_SYSCALL_PROC_CREATE 8
#define SOS_SYSCALL_PROC_DELETE 9
#define SOS_SYSCALL_MY_ID       10
#define SOS_SYSCALL_PROC_STATUS 11
#define SOS_SYSCALL_PROC_WAIT   12
#define SOS_SYSCALL_USLEEP      13
#define SOS_SYSCALL_STAMP       14

typedef struct {
    pid_t     pid;
    unsigned  size;            /* in pages */
    unsigned  stime;           /* start time in msec since booting */
    char      command[32]; /* Name of exectuable */
} sos_process_t;

void syscall_handlers_init();
void *sos_handle_syscall(void *data);
void *sos_handle_vm_fault(void *data);
