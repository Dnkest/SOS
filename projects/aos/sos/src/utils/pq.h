#pragma once

typedef struct _priority_q *PQ;

/* comparison function should return zero if a has higher priority than b. */
PQ pq_create(int (*comparison)(void *, void *));
int pq_empty(PQ pq);
void pq_push(PQ pq, void *item);
void *pq_front(PQ pq);
void pq_pop(PQ pq);
