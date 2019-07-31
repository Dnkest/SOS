#pragma once

#include <stdint.h>

typedef struct _priority_q *PQ;

PQ pq_create(int (*comparison)(void *, void *));
int pq_empty(PQ pq);
void pq_push(PQ pq, void *item);
void *pq_front(PQ pq);
void pq_pop(PQ pq);
void *pq_remove(PQ pq, int (*comparison)(void *, void *), void *value);
void pq_destroy(PQ pq);
void pq_debug(PQ pq, int (*debug)(void *));
