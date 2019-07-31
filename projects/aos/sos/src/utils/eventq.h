#pragma once

#include <picoro/picoro.h>

void eventQ_init();

void eventQ_produce(void *fun(void *arg), void *data);
void eventQ_consume();
void eventQ_cleanup(void *proc);
