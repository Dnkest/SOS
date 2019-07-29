#include <picoro/picoro.h>
#include <sys/types.h>
#include <stdio.h>

#include "syscall.h"
#include "proc.h"
#include "vmem_layout.h"
#include "fs/vfs.h"
#include "globals.h"
#include "uio.h"

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
    seL4_Word mode = arg1;
    seL4_Word len = arg2;

    uio_t *uio = uio_init(proc);
    const char *path_vaddr = (const char *)uio_map(uio, path, len);
    seL4_SetMR(0, vfs_open(process_fdt(proc), path_vaddr, (fmode_t)mode));
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
    seL4_SetMR(0, vfs_write(process_fdt(proc), (int)file, buf_vaddr, (size_t)len));
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
    seL4_SetMR(0, vfs_read(process_fdt(proc), (int)file, buf_vaddr, (size_t)len));
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
    seL4_SetMR(0, vfs_close(process_fdt(proc), (int)file));
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
}

void *sos_handle_syscall(void *data)
{
    proc_t *proc = (proc_t *)data;
    seL4_Word syscall_number = process_get_data0(proc);
    if (syscall_number < SYSCALL_MAX) {
        printf("got syscall %u\n", syscall_number);
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
    printf("faultaddress-> %p\n", fault_address);

    seL4_Word vaddr = fault_address & MASK;
    addrspace_t *addrspace = process_addrspace(proc);
    if (fault_address && addrspace_check_valid_region(addrspace, fault_address)) {

        vframe_ref_t vframe = addrspace_lookup_vframe(addrspace, fault_address);
        if (vframe == 0) {
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
            vframe = addrspace_lookup_vframe(addrspace, fault_address);
        }
        assert(vframe != 0);
        frame_ref_from_v(vframe);
        process_reply(proc, 0);
    } else {
        printf("vm fault on address %p", fault_address);
    }
    //ZF_LOGF("vm fault on address %p", fault_address);
}
