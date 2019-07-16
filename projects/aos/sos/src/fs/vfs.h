#pragma once

#include <stdlib.h>

#include "fd_table.h"
#include "fs_types.h"

int vfs_open(fd_table_t *table, const char *path, fmode_t mode);
int vfs_write(fd_table_t *table, int file, const char *buf, size_t nbyte);
int vfs_read(fd_table_t *table, int file, char *buf, size_t nbyte);
int vfs_getdirent(int pos, char *name, size_t nbyte);
int vfs_stat(const char *path, sos_stat_t *buf);

int vfs_ready();
void vfs_set_ready();

void set_context(struct nfs_context *context);
