#pragma once

typedef struct _queue *Q;

Q q_create();
int q_empty(Q q);
void q_push(Q q, void *item);
void *q_front(Q q);
void q_pop(Q q);
void *q_remove(Q q, int (*comparison)(void *, void *), void *data);
void q_debug(Q q, void (*debug)(void *));
