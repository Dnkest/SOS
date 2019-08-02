#include <stdio.h>
#include "vmq.h"
#include "q.h"
#include "kmalloc.h"

static Q vmQ;

typedef struct vm {
    coro c;
    void *data;
    //int priority;
} vm_t;

// int cmp(void *a, void *b)
// {
//     if (((vm_t *)a)->priority < ((vm_t *)b)->priority) { return 0; }
//     return 1;
// }

void vmQ_init()
{
    vmQ = q_create();
}

int vmQ_empty()
{
    return q_empty(vmQ);
}

void vmQ_push(coro c, void *data)
{
    vm_t *e = (vm_t *)kmalloc(sizeof(vm_t));
    e->c = c;
    e->data = data;
    //e->priority = priority;
    q_push(vmQ, (void *)e);
}

void vmQ_pop()
{
    void *front = q_front(vmQ);
    q_pop(vmQ);
    kfree(front);
}

void vmQ_produce(void *fun(void *arg), void *data)
{
    coro c = coroutine(fun, 1<<14);
    vmQ_push(c, data);
}

void vmQ_consume()
{
    if (!vmQ_empty()) {
        vm_t *front = (vm_t *)q_front(vmQ);
        resume(front->c, front->data);
        if (resumable(front->c)) {
            // q_push(vmQ, (void *)front);
            // q_pop(vmQ);
        } else {
            vmQ_pop();
        }
    }
}

static int comparison(void *a, void *b)
{
    return ((vm_t *)a)->data == b;
}

static void debug(void *a)
{
    printf("%p{%p}->", a, ((vm_t *)a)->data);
}

void vmQ_cleanup(void *proc)
{
    //printf("vm");
    //q_debug(vmQ, debug);
    vm_t *e = (vm_t *)q_remove(vmQ, comparison, proc);
    if (e == NULL) { return; }
    // while (e != NULL && resumable(e->c)) {
    //     resume(e->c, -1);
    // }
    kfree(e);
    //printf("vm");
    //q_debug(vmQ, debug);
}
