#pragma once

#include <stdlib.h>

#include "vnode.h"
#include "fs_types.h"
#include "../utils/low_avail_id.h"

#define MAX_FD  255

typedef struct fd_entry {
    fmode_t mode;
    char *path;
    size_t offset;
    size_t size;
    vnode_t *vnode;
} fd_entry_t;

typedef struct fd_table {
    fd_entry_t *entries[MAX_FD];
    low_avail_id_t *idalloc;
} fd_table_t;

fd_table_t *fdt_init();
int fdt_insert(fd_table_t *table, const char *path, size_t size, vnode_t *vnode, fmode_t mode);
int fdt_delete(fd_table_t *table, int file);
int fdt_entry_exists(fd_table_t *table, int file);
