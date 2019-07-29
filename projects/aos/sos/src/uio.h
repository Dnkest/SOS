#pragma once

#include "proc.h"

typedef struct uio uio_t;

uio_t *uio_init(proc_t *proc);
seL4_Word uio_map(uio_t *uio, seL4_Word user_vaddr, seL4_Word size);
void uio_destroy(uio_t *uio);
