/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sos.h>
#include <stdio.h>

#include <sel4/sel4.h>

int sos_sys_open(const char *path, fmode_t mode)
{
    seL4_SetMR(0, SOS_SYSCALL_OPEN);
    seL4_SetMR(1, path);
    seL4_SetMR(2, mode);
    seL4_SetMR(3, strlen(path));
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 4);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

int sos_sys_close(int file)
{
    seL4_SetMR(0, SOS_SYSCALL_CLOSE);
    seL4_SetMR(1, file);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 2);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

int sos_sys_read(int file, char *buf, size_t nbyte)
{
    seL4_SetMR(0, SOS_SYSCALL_READ);
    seL4_SetMR(1, nbyte);
    seL4_SetMR(2, (seL4_Word)buf);
    seL4_SetMR(3, file);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 4);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

int sos_sys_write(int file, const char *buf, size_t nbyte)
{
    seL4_SetMR(0, SOS_SYSCALL_WRITE);
    seL4_SetMR(1, nbyte);
    seL4_SetMR(2, (seL4_Word)buf);
    seL4_SetMR(3, file);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 4);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

int sos_getdirent(int pos, char *name, size_t nbyte)
{
    seL4_SetMR(0, SOS_SYSCALL_GETDIRENT);
    seL4_SetMR(1, pos);
    seL4_SetMR(2, name);
    seL4_SetMR(3, nbyte);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 4);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

int sos_stat(const char *path, sos_stat_t *buf)
{
    seL4_SetMR(0, SOS_SYSCALL_STAT);
    seL4_SetMR(1, path);
    seL4_SetMR(2, strlen(path));
    seL4_SetMR(3, buf);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 4);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

pid_t sos_process_create(const char *path)
{
    seL4_SetMR(0, SOS_SYSCALL_PROC_CREATE);
    seL4_SetMR(1, path);
    seL4_SetMR(2, strlen(path));
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 3);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

int sos_process_delete(pid_t pid)
{
    seL4_SetMR(0, SOS_SYSCALL_PROC_DELETE);
    seL4_SetMR(1, pid);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 2);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

pid_t sos_my_id(void)
{
    seL4_SetMR(0, SOS_SYSCALL_MY_ID);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

int sos_process_status(sos_process_t *processes, unsigned max)
{
    seL4_SetMR(0, SOS_SYSCALL_PROC_STATUS);
    seL4_SetMR(1, processes);
    seL4_SetMR(2, max);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 3);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

pid_t sos_process_wait(pid_t pid)
{
    seL4_SetMR(0, SOS_SYSCALL_PROC_WAIT);
    seL4_SetMR(1, pid);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 2);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}

void sos_sys_usleep(int msec)
{
    seL4_SetMR(0, SOS_SYSCALL_USLEEP);
    seL4_SetMR(1, msec);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 2);
    seL4_Call(SOS_IPC_EP_CAP, tag);
}

int64_t sos_sys_time_stamp(void)
{
    seL4_SetMR(0, SOS_SYSCALL_STAMP);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Call(SOS_IPC_EP_CAP, tag);

    return seL4_GetMR(0);
}
