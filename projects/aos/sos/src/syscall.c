#include <adt/linkedlist.h>
#include <adt/queue.h>
#include <picoro/picoro.h>

#include "syscall.h"
#include "proc.h"
#include "drivers/serial.h"
#include <aos/sel4_zf_logif.h>
#include "vmem_layout.h"
#include "io_mapping.h"
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
static as_page_table_t *global_pagetable;

typedef struct job {
    coro c;
    void *state;
} job_t;

static Q job_queue;

typedef void *(*syscall_handler_t)(void *);
static syscall_handler_t handlers[SYSCALL_MAX];

void *syscall_open_handler(void *cur_proc)
{
    pcb_t *proc = (pcb_t *)cur_proc;
    seL4_SetMR(0, vfs_open(&seL4_GetIPCBuffer()->msg[2], seL4_GetMR(1)));
    
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(((pcb_t *)cur_proc)->reply, reply_msg);

    return NULL;
}

void *syscall_write_handler(void *cur_proc)
{
    pcb_t *proc = (pcb_t *)cur_proc;

    as_page_table_t *table = proc->as->as_page_table;
    
    seL4_Word vaddr = seL4_GetMR(2);
    seL4_Word size = seL4_GetMR(1);
    seL4_Word base_vaddr = vaddr & MASK;

    int num_pages;
    seL4_Word local_vaddr = alloc_sos_vaddr(vaddr, size, &num_pages);

    seL4_CPtr frame_caps[num_pages];
    unsigned int index = 0;

    for (int i = 0; i < num_pages; i++) {
        seL4_Word tmp_vaddr = local_vaddr;

        seL4_CPtr frame_cap = cspace_alloc_slot(global_cspace);
        if (frame_cap == seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for stack");
            return 0;
        }

        /* copy the frame cap into the slot */
        seL4_Error err = cspace_copy(global_cspace, frame_cap, global_cspace,
                                lookup_frame(table, base_vaddr), seL4_AllRights);
        if (err != seL4_NoError) {
            cspace_free_slot(global_cspace, frame_cap);
            ZF_LOGE("Failed to copy cap");
            return 0;
        }

        /* map it into local vspace */
        err = sos_map_frame(global_cspace, global_vspace, frame_cap, NULL, global_pagetable, tmp_vaddr,
                                seL4_AllRights, seL4_ARM_Default_VMAttributes);
        if (err) {
            printf("error = %d\n", err);
        }

        frame_caps[index++] = frame_cap;

        base_vaddr += PAGE_SIZE_4K;
        tmp_vaddr += PAGE_SIZE_4K;
    }

    vfs_write(seL4_GetMR(3), local_vaddr + (vaddr - (vaddr & MASK)), size);
    memset(local_vaddr, 0, size);

    for (unsigned int i = 0; i < index; i++) {
        seL4_ARM_Page_Unmap(frame_caps[i]);
        cspace_delete(global_cspace, frame_caps[i]);
        cspace_free_slot(global_cspace, frame_caps[i]);
    }

    seL4_SetMR(0, size);
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(proc->reply, reply_msg);
}

void *syscall_read_handler(void *cur_proc)
{
    pcb_t *proc = (pcb_t *)cur_proc;
    set_cur_proc(proc);

    as_page_table_t *table = proc->as->as_page_table;
    
    seL4_Word vaddr = seL4_GetMR(2);
    seL4_Word size = seL4_GetMR(1);
    seL4_Word base_vaddr = vaddr & MASK;

    int num_pages;
    seL4_Word local_vaddr = alloc_sos_vaddr(vaddr, size, &num_pages);

    seL4_CPtr frame_caps1[num_pages];
    seL4_CPtr frame_caps2[num_pages];
    unsigned int index = 0;

    seL4_Word local_vaddr_tmp = local_vaddr;
    for (int i = 0; i < num_pages; i++) {
        
        frame_ref_t frame = alloc_frame();
        if (frame == NULL_FRAME) {
            ZF_LOGE("Couldn't allocate additional stack frame");
            return 0;
        }

        seL4_CPtr local_frame_cap = cspace_alloc_slot(global_cspace);
        if (local_frame_cap == seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for stack");
            return 0;
        }

        seL4_Error err = cspace_copy(global_cspace, local_frame_cap, global_cspace,
                                frame_page(frame), seL4_AllRights);
        if (err != seL4_NoError) {
            cspace_free_slot(global_cspace, local_frame_cap);
            ZF_LOGE("Failed to copy cap");
            return 0;
        }

        err = sos_map_frame(global_cspace, global_vspace, local_frame_cap, NULL, global_pagetable, local_vaddr_tmp,
                                seL4_AllRights, seL4_ARM_Default_VMAttributes);
        if (err) {
            cspace_delete(global_cspace, local_frame_cap);
            cspace_free_slot(global_cspace, local_frame_cap);
            return 0;
        }

        frame_caps1[i] = local_frame_cap;
        local_vaddr_tmp += PAGE_SIZE_4K;
    }

    int actuall_size = vfs_read(seL4_GetMR(3), local_vaddr + (vaddr - (vaddr & MASK)), size);

    seL4_Word target_vaddr = alloc_sos_vaddr(vaddr, size, &num_pages);
    seL4_Word target_tmp = target_vaddr;
    for (int i = 0; i < num_pages; i++) {
        
        seL4_CPtr frame_cap = cspace_alloc_slot(global_cspace);
        if (frame_cap == seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for stack");
            return 0;
        }

        /* copy the frame cap into the slot */
        seL4_Error err = cspace_copy(global_cspace, frame_cap, global_cspace,
                                    lookup_frame(table, base_vaddr), seL4_AllRights);
        if (err != seL4_NoError) {
            cspace_free_slot(global_cspace, frame_cap);
            ZF_LOGE("Failed to copy cap");
            return 0;
        }

        /* map it into local vspace */
        err = sos_map_frame(global_cspace, global_vspace, frame_cap, NULL, global_pagetable, target_tmp,
                                seL4_AllRights, seL4_ARM_Default_VMAttributes);
        if (err) {
            return 0;
        }

        frame_caps2[i] = frame_cap;
        target_tmp += PAGE_SIZE_4K;
    }

    memcpy(target_vaddr + (vaddr - (vaddr & MASK)), local_vaddr + (vaddr - (vaddr & MASK)), 3);

    for (unsigned int i = 0; i < index; i++) {
        seL4_ARM_Page_Unmap(frame_caps1[i]);
        seL4_ARM_Page_Unmap(frame_caps2[i]);
        cspace_delete(global_cspace, frame_caps1[i]);
        cspace_free_slot(global_cspace, frame_caps2[i]);
        cspace_delete(global_cspace, frame_caps1[i]);
        cspace_free_slot(global_cspace, frame_caps2[i]);
    }

    seL4_SetMR(0, actuall_size);
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
    seL4_Send(((pcb_t *)cur_proc)->reply, reply_msg);
}

void syscall_handler_init(cspace_t *cspace, seL4_CPtr vspace, as_page_table_t *pagetable)
{
    global_cspace = cspace;
    global_vspace = vspace;
    global_pagetable = pagetable;
    job_queue = q_create(5);
    handlers[SOS_SYSCALL_OPEN] = syscall_open_handler;
    handlers[SOS_SYSCALL_WRITE] = syscall_write_handler;
    handlers[SOS_SYSCALL_READ] = syscall_read_handler;
    handlers[SOS_SYSCALL_BRK] = syscall_brk_handler;
}

void dispatch_syscall(int syscall_number)
{
    pcb_t *cur_proc = get_cur_proc();
    seL4_CPtr reply = cspace_alloc_slot(cur_proc->global_cspace);
    seL4_Error err = cspace_save_reply_cap(cur_proc->global_cspace, reply);
    ZF_LOGF_IFERR(err, "Failed to save reply");

    cur_proc->reply = reply;

    coro c = coroutine(handlers[syscall_number], BIT(14));
    void *state = resume(c, (void *)cur_proc);

    if (resumable(c)) {
        job_t *new_job = malloc(sizeof(job_t));
        new_job->c = c;
        new_job->state = state;
        q_push(job_queue, new_job);
    }

}

void sos_handle_syscall()
{
    seL4_Word syscall_number = seL4_GetMR(0);
    if (syscall_number < SYSCALL_MAX) {
        dispatch_syscall(syscall_number);
    } else {
        ZF_LOGE("Unknown syscall %lu\n", syscall_number);
    }
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