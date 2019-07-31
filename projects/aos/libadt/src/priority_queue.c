#include <stdio.h>
#include <stdlib.h>
#include <adt/priority_queue.h>

#define SIZE 20

typedef struct _priority_q
{
    int size;
    int capacity;
    void **arr;
    int (*comparison)(void *, void *);
} PriorityQueue;

void bottom_up(PQ pq, int index);
void top_down(PQ pq, int index);

PQ pq_create(int (*comparison)(void *, void *))
{
    PQ pq = (PQ) malloc(sizeof(PriorityQueue));

    pq->size = 0;
    pq->arr = (void **) calloc(SIZE, sizeof(void *));
    pq->capacity = SIZE;
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
    if (pq->size == pq->capacity) {
        pq->capacity *= 2;
        pq->arr = (void **) realloc(pq->arr, pq->capacity *sizeof(void *));
    }
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
    pq->arr[0] = pq->arr[--pq->size];
    top_down(pq, 0);
}

void *pq_remove(PQ pq, int (*comparison)(void *, void *), void *value)
{
    for (int i = 0; i < pq->size; i++) {
        if (comparison(value, pq->arr[i])) {
            pq->arr[i] = pq->arr[--pq->size];
            void *tmp = pq->arr[i];
            bottom_up(pq, i);
            if (tmp == pq->arr[i]) {
                top_down(pq, i);
            }
            return tmp;
        }
    }
    return NULL;
}

void pq_destroy(PQ pq)
{
    free(pq->arr);
    free(pq);
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

void pq_debug(PQ pq, int (*debug)(void *))
{
    for (int i = 0; i < pq->size; i++) {
        debug(pq->arr[i]);
    }
}
