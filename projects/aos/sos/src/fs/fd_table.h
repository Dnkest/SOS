#pragma once

#include <stdlib.h>

#include "vnode.h"

#define MAX_FD  255

typedef struct fd_entry {
    const char *path;
    int flags;
    size_t offset;
    size_t size;
    vnode_t *vnode;
} fd_entry_t;

typedef struct fd_table fd_table_t;

fd_table_t *fdt_init();
int fdt_insert(fd_table_t *table, const char *path, int flags, size_t size, vnode_t *vnode);
int fdt_delete(fd_table_t *table, int file);
fd_entry_t *fdt_get_entry(fd_table_t *table, int file);
void fdt_destroy(fd_table_t *table);
