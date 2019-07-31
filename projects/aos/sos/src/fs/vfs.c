#include <stdio.h>
#include <picoro/picoro.h>
#include <string.h>

#include "vnode.h"
#include "serial.h"
#include "sos_nfs.h"
#include "vfs.h"
#include "fd_table.h"

int vfs_open(const char *path, int flags, vnode_t **vnode)
{
    if (strcmp(path, "console") == 0) {
        //printf("openning console %x\n", flags);

        *vnode = vnode_init(serial_close, serial_read, serial_write);
        while (serial_open(flags) < 0) { yield(0); }
        (*vnode)->size = 0xffffffffffffffff;

        return 0;

    } else {
        //printf("openning %s %x\n", path, flags);

        *vnode = vnode_init(sos_nfs_close, sos_nfs_read, sos_nfs_write);

        if (sos_nfs_open(&((*vnode)->fh), path, flags)) { return -1; }

        sos_stat_t stat_buf;
        if (sos_nfs_stat(path, &stat_buf)) { return -1; }
        //printf("%s has file size %lu\n", path, stat_buf.st_size);
        (*vnode)->size = stat_buf.st_size;

        return 0;
    }
}

int vfs_close(vnode_t *vnode)
{
    return vnode->close_f(vnode->fh);
}

int vfs_write(vnode_t *vnode, const char *buf, size_t offset, size_t nbyte)
{
    struct nfsfh *fh = vnode->fh;
    int written = vnode->write_f(fh, buf, offset, nbyte);
    return written;
}

int vfs_read(vnode_t *vnode, char *buf, size_t offset, size_t nbyte)
{
    struct nfsfh *fh = vnode->fh;
    int read = vnode->read_f(fh, buf, offset, nbyte);
    return read;
}

int vfs_getdirent(int pos, char *name, size_t nbyte)
{
    return sos_nfs_getdirent(pos, name, nbyte);
}

int vfs_stat(const char *path, sos_stat_t *buf)
{
    return sos_nfs_stat(path, buf);
}
