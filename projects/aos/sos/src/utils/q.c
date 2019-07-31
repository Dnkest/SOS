#include <stdio.h>
#include "q.h"
#include "kmalloc.h"

#define SIZE 10

typedef struct _queue
{
    int size;
    int capacity;
    void **arr;
} Queue;

Q q_create()
{
    Q q = (Q)kmalloc(sizeof(Queue));

    q->size = 0;
    q->arr = (void **)kmalloc(SIZE *sizeof(void *));
    q->capacity = SIZE;

    return q;
}

int q_empty(Q q)
{
    return (q->size == 0);
}

void q_push(Q q, void *item)
{
    q->arr[q->size++] = item;
    if (q->size == q->capacity) {
        q->capacity *= 2;
        q->arr = (void **)krealloc(q->arr, q->capacity * sizeof(void *));
    }
}

void *q_front(Q q)
{
    if (q->size > 0) {
        return q->arr[0];
    }
    return NULL;
}

void q_pop(Q q)
{
    if (q->size == 0) return;
    q->size--;
    for (int i = 0; i < q->size; i++) {
        q->arr[i] = q->arr[i+1];
    }
}

void *q_remove(Q q, int (*comparison)(void *, void *), void *data)
{
    for (int i = 0; i < q->size; i++) {
        if (comparison(q->arr[i], data)) {
            void *ret = q->arr[i];
            q->size--;
            for (int j = i; j < q->size; j++) {
                q->arr[j] = q->arr[j+1];
            }
            return ret;
        }
    }
}

void q_debug(Q q, void (*debug)(void *))
{
    for (int i =0 ; i < q->size; i++) {
        debug(q->arr[i]);
    }
    printf("\n");
}
