#pragma once

typedef struct _queue *Q;

Q q_create(int capacity);
int q_empty(Q q);
void q_push(Q q, void *item);
void *q_front(Q q);
void q_pop(Q q);
