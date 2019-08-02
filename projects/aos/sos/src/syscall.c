#include <picoro/picoro.h>
#include <sys/types.h>
#include <clock/clock.h>
#include <stdio.h>
#include <fcntl.h>

#include "fs/vnode.h"
#include "syscall.h"
#include "proc.h"
#include "vmem_layout.h"
#include "fs/vfs.h"
#include "globals.h"
#include "uio.h"
#include "vframe_table.h"
#include "utils/eventq.h"
#include "utils/vmq.h"

#define MASK 0xfffffffff000

static uintptr_t morecore_base = (uintptr_t) PROCESS_HEAP_BASE;
static uintptr_t morecore_top = (uintptr_t) PROCESS_HEAP_TOP;

typedef void (*syscall_handler_t)(proc_t *, seL4_Word, seL4_Word, seL4_Word);
static syscall_handler_t handlers[SYSCALL_MAX];

seL4_Word buffer_size_trunc(seL4_Word size)
{
    if (size >= (1 << 18)) {
        return 0b101111111111111111;
    }
    return size;
}

void syscall_open_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word path = arg0;
    seL4_Word flags = arg1;
    seL4_Word len = arg2;

    uio_t *uio = uio_init(proc);
    const char *path_vaddr = (const char *)uio_map(uio, path, len);

    vnode_t *vnode;
    if (vfs_open(path_vaddr, (int)flags, &vnode) == 0) {
        fd_table_t *fdt = process_fdt(proc);
        seL4_SetMR(0, fdt_insert(fdt, path_vaddr, (int)flags, vnode->size, vnode));
    } else {
        seL4_SetMR(0, -1);
    }
    uio_destroy(uio);
    process_reply(proc, 1);
}

void syscall_write_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word len = buffer_size_trunc(arg0);
    seL4_Word buf = arg1;
    seL4_Word file = arg2;

    uio_t *uio = uio_init(proc);
    const char *buf_vaddr = (const char *)uio_map(uio, buf, len);
    fd_table_t *fdt = process_fdt(proc);
    fd_entry_t *e = fdt_get_entry(fdt, (int)file);
    if (e == NULL) {
        seL4_SetMR(0, -1);
    } else {
        vnode_t *vnode = e->vnode;
        int written = vfs_write(vnode, buf_vaddr, e->offset, (size_t)len);
        written = ((size_t)written < (size_t)len) ? written : (size_t)len;
        e->offset += written;
        seL4_SetMR(0, written);
    }
    uio_destroy(uio);
    process_reply(proc, 1);
}

void syscall_read_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word len = buffer_size_trunc(arg0);
    seL4_Word buf = arg1;
    seL4_Word file = arg2;

    uio_t *uio = uio_init(proc);
    char *buf_vaddr = (char *)uio_map(uio, buf, len);
    fd_table_t *fdt = process_fdt(proc);
    fd_entry_t *e = fdt_get_entry(fdt, (int)file);
    if (e == NULL) {
        seL4_SetMR(0, -1);
    } else {
        vnode_t *vnode = e->vnode;
        size_t offset = e->offset;
        size_t nbyte = (size_t)len;
        if (offset >= e->size) {
            seL4_SetMR(0, 0);
        } else {
            if (e->size - offset < nbyte) {
                nbyte = e->size - offset;
            }
            int read = vfs_read(vnode, buf_vaddr, offset, nbyte);
            read = ((size_t)read < nbyte) ? read : (int)nbyte;
            e->offset += read;
            seL4_SetMR(0, read);
        }
    }
    uio_destroy(uio);
    process_reply(proc, 1);
}

void syscall_brk_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    uintptr_t ret;
    uintptr_t newbrk = arg0;

    /*if the newbrk is 0, return the bottom of the heap*/
    if (!newbrk) {
        ret = morecore_base;
    } else if (newbrk < morecore_top && newbrk > morecore_base) {
        ret = morecore_base = newbrk;
    } else {
        ret = 0;
    }

    seL4_SetMR(0, ret);
    process_reply(proc, 1);
}

void syscall_getdirent_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word pos = arg0;
    seL4_Word name = arg1;
    seL4_Word nbyte = arg2;

    uio_t *uio = uio_init(proc);
    char *name_vaddr = (char *)uio_map(uio, name, nbyte);
    seL4_SetMR(0, vfs_getdirent((int)pos, name_vaddr, (size_t )nbyte));
    uio_destroy(uio);
    process_reply(proc, 1);
}

void syscall_stat_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word path = arg0;
    seL4_Word len= arg1;
    seL4_Word buf = arg2;

    uio_t *uio_path = uio_init(proc);
    const char *path_vaddr = (const char *)uio_map(uio_path, path, len);
    uio_t *uio_buf = uio_init(proc);
    char *buf_vaddr = (char *)uio_map(uio_buf, buf, sizeof(sos_stat_t));
    seL4_SetMR(0, vfs_stat(path_vaddr, buf_vaddr));
    uio_destroy(uio_path);
    uio_destroy(uio_buf);
    process_reply(proc, 1);
}

void syscall_close_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word file = arg0;

    fd_table_t *fdt = process_fdt(proc);
    fd_entry_t *e = fdt_get_entry(fdt, (int)file);
    if (e == NULL) {
        seL4_SetMR(0, -1);
    } else {
        vnode_t *vnode = e->vnode;
        seL4_SetMR(0, vfs_close(vnode));
    }
    process_reply(proc, 1);
}

void syscall_process_create_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word path = arg0;
    seL4_Word len = arg1;

    uio_t *uio = uio_init(proc);
    char *path_vaddr = (char *)uio_map(uio, path, len);
    seL4_SetMR(0, process_init(path_vaddr, ipc_ep()));
    uio_destroy(uio);
    process_reply(proc, 1);
}

void syscall_process_delete_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word pid = arg0;
    if (process_exists_by_id((int)pid)) {
        // printf("deleting %d(%p), im %d(%p)\n", (int)pid, process_get_by_id((int)pid), 
        // process_id(proc), proc);

        proc_t *deletee = process_get_by_id((int)pid);
        if (deletee == proc) {
            //printf("deleting itself\n");
            process_set_exiting((int)pid);
            return;
        } else {
            //printf("deletee %p\n", deletee);
            eventQ_cleanup((void *)deletee);
            vmQ_cleanup((void *)deletee);
            process_delete(deletee);
            process_reply(proc, 1);
        }
    } else {
        seL4_SetMR(0, -1);
    }
    process_reply(proc, 1);
}

void syscall_process_id_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_SetMR(0, process_id(proc));
    process_reply(proc, 1);
}

void syscall_process_status_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word buf = arg0;
    seL4_Word max = arg1;

    uio_t *uio = uio_init(proc);
    sos_process_t *buf_vaddr = (sos_process_t *)uio_map(uio, buf, (size_t)max *sizeof(sos_process_t));
    int i = 0, n = 0;
    while (i < (int)max && n < process_max()) {
        proc_t *tmp;
        if (process_exists_by_id(n)) {
            tmp = process_get_by_id(n);
            buf_vaddr[i].pid = process_id(tmp);
            buf_vaddr[i].size = process_size(tmp);
            buf_vaddr[i].stime = process_time(tmp);
            strncpy(buf_vaddr[i].command, process_name(tmp), 30);
            i++;
        }
        n++;
    }
    seL4_SetMR(0, i);
    uio_destroy(uio);
    process_reply(proc, 1);
}

void syscall_process_wait_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_Word pid = arg0;
    while (!process_id_exits((int)pid) || ((int)pid == -1 && !process_any_exits())) {
        yield(0);
    }
    seL4_SetMR(0, pid);
    process_reply(proc, 1);
    syscall_process_delete_handler(proc, pid, 0, 0);

}

void sleep_callback(uint32_t id, void *data)
{
    process_reply((proc_t *)data, 1);
}

void syscall_time_usleep_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    register_timer(arg0 * 1000, sleep_callback, (void *)proc);
}

void syscall_time_stamp_handler(proc_t *proc, seL4_Word arg0, seL4_Word arg1, seL4_Word arg2)
{
    seL4_SetMR(0, get_time());
    process_reply(proc, 1);
}

void syscall_handlers_init()
{
    handlers[SOS_SYSCALL_OPEN] = syscall_open_handler;
    handlers[SOS_SYSCALL_WRITE] = syscall_write_handler;
    handlers[SOS_SYSCALL_READ] = syscall_read_handler;
    handlers[SOS_SYSCALL_BRK] = syscall_brk_handler;
    handlers[SOS_SYSCALL_GETDIRENT] = syscall_getdirent_handler;
    handlers[SOS_SYSCALL_STAT] = syscall_stat_handler;
    handlers[SOS_SYSCALL_CLOSE] = syscall_close_handler;
    handlers[SOS_SYSCALL_PROC_CREATE] = syscall_process_create_handler;
    handlers[SOS_SYSCALL_PROC_DELETE] = syscall_process_delete_handler;
    handlers[SOS_SYSCALL_MY_ID] = syscall_process_id_handler;
    handlers[SOS_SYSCALL_PROC_STATUS] = syscall_process_status_handler;
    handlers[SOS_SYSCALL_PROC_WAIT] = syscall_process_wait_handler;
    handlers[SOS_SYSCALL_USLEEP] = syscall_time_usleep_handler;
    handlers[SOS_SYSCALL_STAMP] = syscall_time_stamp_handler;
}

void *sos_handle_syscall(void *data)
{
    proc_t *proc = (proc_t *)data;
    seL4_Word syscall_number = process_get_data0(proc);
    if (syscall_number <= SYSCALL_MAX) {
        handlers[syscall_number](proc,
                                process_get_data1(proc),
                                process_get_data2(proc),
                                process_get_data3(proc));
    } else {
        ZF_LOGE("Unknown syscall %lu\n", syscall_number);
    }
}

void *sos_handle_vm_fault(void *data)
{
    proc_t *proc = (proc_t *)data;
    seL4_Word fault_address = process_get_data0(proc);
//printf("fault vaddr %p\n", fault_address);
    seL4_Word vaddr = fault_address & MASK;
    addrspace_t *addrspace = process_addrspace(proc);
    if (fault_address && addrspace_check_valid_region(addrspace, vaddr)) {
        if (!addrspace_vaddr_exists(addrspace, vaddr)) {
            //printf("proc %d needs alloc new\n", process_id(proc));
            seL4_CPtr frame_cap = cspace_alloc_slot(global_cspace());
            if (frame_cap == seL4_CapNull) {
                free_frame(frame_cap);
                ZF_LOGE("Failed to alloc slot for frame");
            }

            seL4_Error err = addrspace_alloc_map_one_page(addrspace, global_cspace(),
                                frame_cap, process_vspace(proc), vaddr);
            if (err) {
                ZF_LOGE("Failed to copy cap");
            }
            
        }
        vframe_ref_t vframe = addrspace_lookup_vframe(addrspace, vaddr);
//printf("proc %d vframe %u\n", process_id(proc), vframe);
        frame_from_vframe(vframe);
//printf("proc %d resolved\n", process_id(proc));
        process_reply(proc, 0);
    } else {
        printf("process %d vm fault %p\n", process_id(proc), fault_address);
    }
}
