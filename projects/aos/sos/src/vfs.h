#pragma once

#include <stdint.h>

typedef int (*vfs_read_t)(void *buf, int len);
typedef int (*vfs_write_t)(void *buf, int len);

int vfs_open(const char *path, int flags);
int vfs_write(int file, const char *buf, size_t nbyte);
int vfs_read(int file, char *buf, size_t nbyte);
