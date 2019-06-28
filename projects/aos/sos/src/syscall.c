#include "syscall.h"
#include "proc.h"
#include "drivers/serial.h"
#include "picoro.h"
#include <aos/sel4_zf_logif.h>

#define SYSCALL_MAX             5
#define SOS_SYSCALL_OPEN        1
#define SOS_SYSCALL_WRITE       2
#define SOS_SYSCALL_READ        3

#define MAX_COROUTINES          20

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
    serial_write((char *)&seL4_GetIPCBuffer()->msg[2], seL4_GetMR(1));

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