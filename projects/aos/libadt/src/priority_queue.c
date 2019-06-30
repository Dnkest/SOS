#include <stdio.h>
#include <stdlib.h>
#include <adt/priority_queue.h>

typedef struct _priority_q
{
    int size;
    int capacity;
    void **arr;
    int (*comparison)(void *, void *);
} PriorityQueue;

void bottom_up(PQ pq, int index);
void top_down(PQ pq, int index);

PQ pq_create(int capacity, int (*comparison)(void *, void *))
{
    PQ pq = (PQ) malloc(sizeof(PriorityQueue));

    pq->size = 0;
    pq->arr = (void **) calloc(capacity, sizeof(void *));
    pq->capacity = capacity;
    pq->comparison = comparison;

    return pq;
}

int pq_empty(PQ pq)
{
    return (pq->size == 0);
}

void pq_push(PQ pq, void *item)
{
    pq->arr[pq->size] = item;
    bottom_up(pq, pq->size);
    pq->size++;
}

void *pq_front(PQ pq)
{
    if (pq->size > 0) {
        return pq->arr[0];
    }
    return 0;
}

void pq_pop(PQ pq)
{
    if (pq->size == 0) return;
    pq->arr[0] = pq->arr[pq->size-1];
    pq->size--;
    top_down(pq, 0);
}

void bottom_up(PQ pq, int index)
{
    if (index == 0) return;

    int parent = (index-1)/2;
    void *tmp;

    if (pq->comparison(pq->arr[parent], pq->arr[index])) {
        tmp = pq->arr[parent];
        pq->arr[parent] = pq->arr[index];
        pq->arr[index] = tmp;
        bottom_up(pq, parent);
    }
}

void top_down(PQ pq, int index)
{
    int left = index*2+1;
    int right = index*2+2;
    
    int min;
    if (left < pq->size && pq->comparison(pq->arr[index], pq->arr[left])) {
        min = left;
    } else {
        min = index;
    }
    if (right < pq->size && pq->comparison(pq->arr[min], pq->arr[right])) {
        min = right;
    }
    if (min == index) return;

    void *tmp;
    tmp = pq->arr[min];
    pq->arr[min] = pq->arr[index];
    pq->arr[index] = tmp;
    top_down(pq, min);
}
