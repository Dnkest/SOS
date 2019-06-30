#pragma once

#include <stdint.h>

typedef struct _priority_q *PQ;

PQ pq_create(int capacity, int (*comparison)(void *, void *));
int pq_empty(PQ pq);
void pq_push(PQ pq, void *item);
void *pq_front(PQ pq);
void pq_pop(PQ pq);
