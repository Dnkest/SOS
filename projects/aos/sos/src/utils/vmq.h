#pragma once

#include <picoro/picoro.h>

void vmQ_init();

int vmQ_empty();
void vmQ_produce(void *fun(void *arg), void *data);
void vmQ_consume();
void vmQ_cleanup(void *proc);
