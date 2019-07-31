#pragma once

#include <stdlib.h>

#include "fd_table.h"
#include "fs_types.h"

int vfs_open(const char *path, int flags, vnode_t **vnode);
int vfs_write(vnode_t *vnode, const char *buf, size_t offset, size_t nbyte);
int vfs_read(vnode_t *vnode, char *buf, size_t offset, size_t nbyte);
int vfs_getdirent(int pos, char *name, size_t nbyte);
int vfs_stat(const char *path, sos_stat_t *buf);
int vfs_close(vnode_t *vnode);
