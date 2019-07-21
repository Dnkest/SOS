#include <picoro/picoro.h>
#include <sys/types.h>
#include <aos/sel4_zf_logif.h>
#include <stdio.h>

#include "syscall.h"
#include "vmem_layout.h"
#include "fs/vfs.h"
#include "utils/kmalloc.h"
#include "utils/jobq.h"
#include "fs/serial.h"

#define MASK 0xfffffffff000

static uintptr_t morecore_base = (uintptr_t) PROCESS_HEAP_BASE;
static uintptr_t morecore_top = (uintptr_t) PROCESS_HEAP_TOP;

static cspace_t *global_cspace;
static seL4_CPtr global_vspace;
static addrspace_t *global_addrspace;

typedef void *(*syscall_handler_t)(void *);
static syscall_handler_t handlers[SYSCALL_MAX];

static int done = 0;

void *syscall_open_handler(void *cur_proc)
{
    process_t *proc = (process_t *)cur_proc;
    seL4_Word path = seL4_GetMR(1);
    seL4_Word mode = seL4_GetMR(2);
    seL4_Word len = seL4_GetMR(4);

    proc_map_t *mapped = (proc_map_t *)kmalloc(sizeof(proc_map_t));
    const char *path_vaddr = (const char *)process_map(proc, path, len,
                    global_addrspace, global_vspace,
                    mapped);

    seL4_SetMR(0, vfs_open(proc->fdt, path_vaddr, (fmode_t)mode));

    process_unmap(proc, global_addrspace, mapped);
    kfree(mapped);
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(proc->reply, reply_msg);
}

void *syscall_write_handler(void *cur_proc)
{
    process_t *proc = (process_t *)cur_proc;
    seL4_Word size = seL4_GetMR(1);
    if (size >= (1 << 18)) {
        size = 0b101111111111111111;
    }
    seL4_Word user_vaddr = seL4_GetMR(2);
    int file = seL4_GetMR(3);
    

    if (size != 0) {
        proc_map_t *mapped = (proc_map_t *)kmalloc(sizeof(proc_map_t));
        seL4_Word vaddr = process_map(proc, user_vaddr, size,
                            global_addrspace, global_vspace,
                            mapped);

        int actual = vfs_write(proc->fdt, file, vaddr, size);

        process_unmap(proc, global_addrspace, mapped);
        kfree(mapped);
        seL4_SetMR(0, actual);
    } else {
        seL4_SetMR(0, 0);
    }
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(proc->reply, reply_msg);
}

void *syscall_read_handler(void *cur_proc)
{
    process_t *proc = (process_t *)cur_proc;
    seL4_Word size = seL4_GetMR(1);
//printf("size %lu\n", size);
    if (size >= (1 << 17)) {
        
        size = 0b101111111111111111;
    }
    seL4_Word user_vaddr = seL4_GetMR(2);
    int file = seL4_GetMR(3);

    if (size != 0) {
        proc_map_t *mapped = (proc_map_t *)kmalloc(sizeof(proc_map_t));
        seL4_Word vaddr = process_map(proc, user_vaddr, size,
                            global_addrspace, global_vspace,
                            mapped);
        int actual = vfs_read(proc->fdt, file, vaddr, size);
        //printf("read actul %d\n", actual);
        process_unmap(proc, global_addrspace, mapped);
        kfree(mapped);
        seL4_SetMR(0, actual);
    } else {
        seL4_SetMR(0, 0);
    }
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(proc->reply, reply_msg);
}

void *syscall_brk_handler(void *cur_proc)
{
    uintptr_t ret;
    uintptr_t newbrk = seL4_GetMR(1);

    /*if the newbrk is 0, return the bottom of the heap*/
    if (!newbrk) {
        ret = morecore_base;
    } else if (newbrk < morecore_top && newbrk > morecore_base) {
        ret = morecore_base = newbrk;
    } else {
        ret = 0;
    }

    seL4_SetMR(0, ret);
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(((process_t *)cur_proc)->reply, reply_msg);
}

void *syscall_getdirent_handler(void *cur_proc)
{
    process_t *proc = (process_t *)cur_proc;
    seL4_Word pos = seL4_GetMR(1);
    seL4_Word name = seL4_GetMR(2);
    seL4_Word nbyte = seL4_GetMR(3);
    proc_map_t *mapped = (proc_map_t *)kmalloc(sizeof(proc_map_t));
    char *name_vaddr = (char *)process_map(proc, name, nbyte,
                    global_addrspace, global_vspace,
                    mapped);
    seL4_SetMR(0, vfs_getdirent((int)pos, name_vaddr, (size_t )nbyte));
    process_unmap(proc, global_addrspace, mapped);
    kfree(mapped);
    
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(proc->reply, reply_msg);
}

void *syscall_stat_handler(void *cur_proc)
{
    process_t *proc = (process_t *)cur_proc;
    seL4_Word path = seL4_GetMR(1);
    seL4_Word buf = seL4_GetMR(2);

    proc_map_t *mapped_path = (proc_map_t *)kmalloc(sizeof(proc_map_t));
    const char *path_vaddr = (const char *)process_map(proc, path, 6144,
                    global_addrspace, global_vspace,
                    mapped_path);
    proc_map_t *mapped_buf = (proc_map_t *)kmalloc(sizeof(proc_map_t));
    sos_stat_t *buf_vaddr = (const char *)process_map(proc, buf, sizeof(sos_stat_t),
                    global_addrspace, global_vspace,
                    mapped_buf);
    seL4_SetMR(0, vfs_stat(path_vaddr, buf_vaddr));
    process_unmap(proc, global_addrspace, mapped_path);
    kfree(mapped_path);
    process_unmap(proc, global_addrspace, mapped_buf);
    kfree(mapped_buf);
    
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(proc->reply, reply_msg);
}

void *syscall_close_handler(void *cur_proc)
{
    process_t *proc = (process_t *)cur_proc;
    seL4_Word file = seL4_GetMR(1);

    seL4_SetMR(0, vfs_close(proc->fdt, (int)file));
    
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(proc->reply, reply_msg);
}

void syscall_handler_init(cspace_t *cspace, seL4_CPtr vspace, addrspace_t *addrspace)
{
    global_cspace = cspace;
    global_vspace = vspace;
    global_addrspace = addrspace;
    jobq_init();
    handlers[SOS_SYSCALL_OPEN] = syscall_open_handler;
    handlers[SOS_SYSCALL_WRITE] = syscall_write_handler;
    handlers[SOS_SYSCALL_READ] = syscall_read_handler;
    handlers[SOS_SYSCALL_BRK] = syscall_brk_handler;
    handlers[SOS_SYSCALL_GETDIRENT] = syscall_getdirent_handler;
    handlers[SOS_SYSCALL_STAT] = syscall_stat_handler;
    handlers[SOS_SYSCALL_CLOSE] = syscall_close_handler;
}

void dispatch_syscall(process_t *proc, int syscall_number)
{
    seL4_CPtr reply = cspace_alloc_slot(proc->global_cspace);
    seL4_Error err = cspace_save_reply_cap(proc->global_cspace, reply);
    ZF_LOGF_IFERR(err, "Failed to save reply");

    proc->reply = reply;

    coro c = coroutine(handlers[syscall_number], BIT(16));
    proc = resume(c, (void *)proc);

    if (resumable(c)) {
        job_t *new_job = kmalloc(sizeof(job_t));
        new_job->c = c;
        new_job->proc = proc;
        jobq_push(new_job);
    }

}

void sos_handle_syscall(process_t *proc)
{
    seL4_Word syscall_number = seL4_GetMR(0);
    if (syscall_number < SYSCALL_MAX) {
        dispatch_syscall(proc, syscall_number);
    } else {
        ZF_LOGE("Unknown syscall %lu\n", syscall_number);
    }
}

bool sos_handle_page_fault(process_t *proc, seL4_Word fault_address)
{
    printf("faultaddress-> %p\n", fault_address);

    seL4_CPtr reply = cspace_alloc_slot(proc->global_cspace);
    seL4_Error err = cspace_save_reply_cap(proc->global_cspace, reply);
    ZF_LOGF_IFERR(err, "Failed to save reply");

    proc->reply = reply;

    seL4_Word vaddr = fault_address & MASK;
    if (fault_address && addrspace_check_valid_region(proc->addrspace, fault_address) || 1) {

        int present = addrspace_set_reference(proc->addrspace, proc->vspace, vaddr);

        if (!present) {
            vframe_ref_t vframe = addrspace_lookup_vframe(proc->addrspace, fault_address);
            if (vframe == 0) {
                seL4_CPtr frame_cap = cspace_alloc_slot(global_cspace);
                if (frame_cap == seL4_CapNull) {
                    free_frame(frame_cap);
                    ZF_LOGE("Failed to alloc slot for frame");
                }

                seL4_Error err = addrspace_alloc_map_one_page(proc->addrspace, global_cspace,
                        frame_cap, proc->vspace, fault_address & MASK);
                if (err) {
                    ZF_LOGE("Failed to copy cap");
                }

            } else {
                frame_ref_from_v(vframe);
            }
        }
        seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Send(proc->reply, reply_msg);
        return true;
    }
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_Send(proc->reply, reply_msg);
    return false;
}


void do_jobs()
{
    if (jobq_empty()) return;

    job_t *job = jobq_front();
    jobq_pop();

    void *proc = resume(job->c, job->proc);

    if (resumable(job->c)) {
        job->proc = proc;
        jobq_push(job);
    } else {
        kfree(job);
    }
}

cspace_t *get_global_cspace()
{
    return global_cspace;
}

seL4_CPtr get_global_vspace()
{
    return global_vspace;
}

addrspace_t *get_global_addrspace()
{
    return global_addrspace;
}
