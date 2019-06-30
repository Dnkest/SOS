#include <stdio.h>
#include <stdlib.h>
#include <adt/queue.h>

typedef struct _queue
{
    int size;
    int capacity;
    void **arr;
} Queue;

Q q_create(int capacity)
{
    Q q = (Q) malloc(sizeof(Queue));

    q->size = 0;
    q->arr = (void **) calloc(capacity, sizeof(void *));
    q->capacity = capacity;

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
        q->arr = (void **) realloc(q->arr, q->capacity * sizeof(void *));
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
