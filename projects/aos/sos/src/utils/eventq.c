#include <stdio.h>
#include "eventq.h"
#include "q.h"
#include "kmalloc.h"

static Q eventQ;

typedef struct event {
    coro c;
    void *data;
    //int priority;
} event_t;

// int cmp(void *a, void *b)
// {
//     if (((event_t *)a)->priority < ((event_t *)b)->priority) { return 0; }
//     return 1;
// }

void eventQ_init()
{
    eventQ = q_create();
}

int eventQ_empty()
{
    return q_empty(eventQ);
}

void eventQ_push(coro c, void *data)
{
    event_t *e = (event_t *)kmalloc(sizeof(event_t));
    e->c = c;
    e->data = data;
    //e->priority = priority;
    q_push(eventQ, (void *)e);
}

void eventQ_pop()
{
    void *front = q_front(eventQ);
    q_pop(eventQ);
    kfree(front);
}

void eventQ_produce(void *fun(void *arg), void *data)
{
    coro c = coroutine(fun, 1<<14);
    eventQ_push(c, data);
}

void eventQ_consume()
{
    if (!eventQ_empty()) {
        event_t *front = (event_t *)q_front(eventQ);

        resume(front->c, front->data);
        if (resumable(front->c)) {
            //printf("keep going\n");
            //q_debug(eventQ);
            q_push(eventQ, (void *)front);
            q_pop(eventQ);
        } else {
            eventQ_pop();
        }
    }
}
