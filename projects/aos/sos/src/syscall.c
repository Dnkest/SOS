#include "syscall.h"
#include "proc.h"
#include "drivers/serial.h"
#include "picoro.h"
#include <aos/sel4_zf_logif.h>
#include "vmem_layout.h"

#define SYSCALL_MAX             5
#define SOS_SYSCALL_OPEN        1
#define SOS_SYSCALL_WRITE       2
#define SOS_SYSCALL_READ        3

#define MAX_COROUTINES          20

#define MASK 0xfffffffff000

typedef struct job {
    coro c;
    void *state;
} job_t;

static job_t queue[MAX_COROUTINES];
static int size = 0;
void push(job_t job);
job_t pop();

typedef void *(*syscall_handler_t)(void *);
static syscall_handler_t handlers[SYSCALL_MAX];

void *syscall_write_handler(void *cur_proc)
{
    pcb_t *proc = (pcb_t *)cur_proc;
    cspace_t *cspace = proc->global_cspace;
    as_page_table_t *table = proc->as->as_page_table;
    
    seL4_Word vaddr = seL4_GetMR(2);
    seL4_Word size = seL4_GetMR(1);

    seL4_Word base_vaddr = vaddr & MASK;
    seL4_Word top_vaddr = (vaddr + size) & MASK;
    if (size != (size & MASK)) {
        top_vaddr += BIT(seL4_PageBits);
    }

    int num_of_pages = (top_vaddr - base_vaddr)/BIT(seL4_PageBits);
    if (num_of_pages > 100) {
        /* too big, TODO */
    }

    /* supporting 100 pages amount of data for now. */
    seL4_CPtr frame_caps[100];
    unsigned int index = 0;

    size_t count = seL4_GetMR(1);
    seL4_Word local_vaddr = SOS_TMP_BUFFER;
    for (int i = 0; i < num_of_pages; i++) {
        /* allocate a slot to duplicate the frame cap so we can map it into our address space */
        seL4_CPtr frame_cap = cspace_alloc_slot(cspace);
        if (frame_cap == seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for stack");
            return 0;
        }

        /* copy the frame cap into the slot */
        seL4_Error err = cspace_copy(cspace, frame_cap, cspace, lookup_frame(table, base_vaddr), seL4_AllRights);
        if (err != seL4_NoError) {
            cspace_free_slot(cspace, frame_cap);
            ZF_LOGE("Failed to copy cap");
            return 0;
        }

        /* map it into local vspace */
        err = sys_map_frame(cspace, frame_cap, seL4_CapInitThreadVSpace, local_vaddr, seL4_AllRights,
                    seL4_ARM_Default_VMAttributes);
        if (err) {
            printf("error = %d\n", err);
        }

        frame_caps[index++] = frame_cap;

        base_vaddr += BIT(seL4_PageBits);
        local_vaddr += BIT(seL4_PageBits);
    }
        
    serial_write(SOS_TMP_BUFFER + (vaddr - (vaddr & MASK)), size);
    memset(SOS_TMP_BUFFER, 0, seL4_GetMR(1));

    for (unsigned int i = 0; i < index; i++) {
        seL4_ARM_Page_Unmap(frame_caps[i]);
        cspace_delete(cspace, frame_caps[i]);
        cspace_free_slot(cspace, frame_caps[i]);
    }

    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_Send(((pcb_t *)cur_proc)->reply, reply_msg);

    return NULL;
}

// void serial_read_continue(serial_t serial, char c)
// {

// }

// void syscall_read_handler()
// {
//     serial_register_read_handler(serial_read_continue);
// }

void syscall_handlers_init()
{
    handlers[SOS_SYSCALL_WRITE] = syscall_write_handler;
    //handlers[SOS_SYSCALL_READ] = syscall_read_handler;
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
        job_t new_job = {c, state};
        push(new_job);
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

void push(job_t job)
{
    if (size == MAX_COROUTINES) return;
    queue[size++] = job;
}

job_t pop()
{
    job_t ret = queue[0];
    size--;
    for (int i = 0; i < size; i++) {
        queue[i] = queue[i+1];
    }
    return ret;
}