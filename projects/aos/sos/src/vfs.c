#include <stdlib.h>
#include <stdio.h>

#include "drivers/serial.h"
#include "vfs.h"

typedef struct iovec {
    vfs_read_t read_f;
    vfs_write_t write_f;
} *iovec_t;

typedef struct open_file {
    iovec_t iov;
    int mode;
} *open_file_t;

static open_file_t fdt[20];
static int index = 3;

int vfs_open(const char *path, int flags)
{
    if (strcmp(path, "console") == 0) {
        open_file_t f = malloc(sizeof(struct open_file));
        f->mode = flags;
        f->iov = malloc(sizeof(struct iovec));

        f->iov->write_f = serial_write;
        f->iov->read_f = serial_read;

        fdt[index++] = f;
        return 3;
    }
}

int vfs_write(int file, const char *buf, size_t nbyte)
{
    return fdt[file]->iov->write_f(buf, nbyte);
}

int vfs_read(int file, char *buf, size_t nbyte)
{
    return fdt[3]->iov->read_f(buf, nbyte);
}
