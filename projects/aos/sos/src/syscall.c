// #include <adt/linkedlist.h>
#include <adt/queue.h>
#include <picoro/picoro.h>

#include "syscall.h"
#include "drivers/serial.h"
#include <aos/sel4_zf_logif.h>
#include "vmem_layout.h"
// #include "io_mapping.h"
#include "vfs.h"

#define SYSCALL_MAX             5
#define SOS_SYSCALL_OPEN        1
#define SOS_SYSCALL_WRITE       2
#define SOS_SYSCALL_READ        3
#define SOS_SYSCALL_BRK         4

#define MASK 0xfffffffff000

static uintptr_t morecore_base = (uintptr_t) PROCESS_HEAP_BASE;
static uintptr_t morecore_top = (uintptr_t) PROCESS_HEAP_TOP;

static cspace_t *global_cspace;
static seL4_CPtr global_vspace;
static addrspace_t *global_addrspace;

typedef struct job {
    coro c;
    void *state;
} job_t;

static Q job_queue;

typedef void *(*syscall_handler_t)(void *);
static syscall_handler_t handlers[SYSCALL_MAX];

void *syscall_open_handler(void *cur_proc)
{
    process_t *proc = (process_t *)cur_proc;
    seL4_SetMR(0, vfs_open(&seL4_GetIPCBuffer()->msg[2], seL4_GetMR(1)));
    
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(((process_t *)cur_proc)->reply, reply_msg);

    return NULL;
}

void *syscall_write_handler(void *cur_proc)
{
    seL4_Word size = seL4_GetMR(1);
    int nbyte = process_write((process_t *)cur_proc, global_addrspace, global_vspace,
                                seL4_GetMR(2), size);
    seL4_SetMR(0, nbyte);
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(((process_t *)cur_proc)->reply, reply_msg);
}

void *syscall_read_handler(void *cur_proc)
{
    seL4_Word size = seL4_GetMR(1);
    int nbyte = process_read((process_t *)cur_proc, global_addrspace, global_vspace,
                                seL4_GetMR(2), size);
    seL4_SetMR(0, nbyte);
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(((process_t *)cur_proc)->reply, reply_msg);
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


void syscall_handler_init(cspace_t *cspace, seL4_CPtr vspace, addrspace_t *addrspace)
{
    global_cspace = cspace;
    global_vspace = vspace;
    global_addrspace = addrspace;
    job_queue = q_create(5);
    handlers[SOS_SYSCALL_OPEN] = syscall_open_handler;
    handlers[SOS_SYSCALL_WRITE] = syscall_write_handler;
    handlers[SOS_SYSCALL_READ] = syscall_read_handler;
    handlers[SOS_SYSCALL_BRK] = syscall_brk_handler;
}

void dispatch_syscall(process_t *proc, int syscall_number)
{
    seL4_CPtr reply = cspace_alloc_slot(proc->global_cspace);
    seL4_Error err = cspace_save_reply_cap(proc->global_cspace, reply);
    ZF_LOGF_IFERR(err, "Failed to save reply");

    proc->reply = reply;

    coro c = coroutine(handlers[syscall_number], BIT(14));

    void *state = resume(c, (void *)proc);

    if (resumable(c)) {
        //printf("resumed\n");
        job_t *new_job = malloc(sizeof(job_t));
        new_job->c = c;
        new_job->state = state;
        q_push(job_queue, new_job);
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
    if (addrspace_check_valid_region(proc->addrspace, fault_address)) {

        seL4_CPtr frame_cap = cspace_alloc_slot(global_cspace);
        if (frame_cap == seL4_CapNull) {
            free_frame(frame_cap);
            ZF_LOGE("Failed to alloc slot for frame");
            return false;
        }

        seL4_Error err = addrspace_alloc_map_one_page(proc->addrspace, global_cspace, frame_cap,
                                   proc->vspace, fault_address & MASK);
        if (err) {
            printf("error = %d\n", err);
            ZF_LOGE("Failed to copy cap");
            return false;
        }
        seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Reply(reply_msg);
        return true;
    }
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_Reply(reply_msg);
    return false;
}


void do_jobs()
{   
    if (q_empty(job_queue)) return;

    job_t *job = q_front(job_queue);
    q_pop(job_queue);

    void *state = resume(job->c, (void *)job->state);

    if (resumable(job->c)) {
        q_push(job_queue, job);
    }
}