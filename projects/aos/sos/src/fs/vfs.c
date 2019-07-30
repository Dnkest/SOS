#include <stdio.h>
#include <picoro/picoro.h>
#include <string.h>

#include "vnode.h"
#include "serial.h"
#include "sos_nfs.h"
#include "vfs.h"
#include "fd_table.h"

int vfs_open(fd_table_t *table, const char *path, fmode_t mode)
{
    if (strcmp(path, "console") == 0) {
        //printf("openning console %x\n", mode);
        vnode_t *vnode = vnode_init(serial_close, serial_read, serial_write);
        if (serial_open(mode) < 0) { return -1; }
        return fdt_insert(table, "console", 0xffffffffffffffff, vnode, mode);
    } else {
        //printf("openning %s %x\n", path, mode);
        vnode_t *vnode = vnode_init(sos_nfs_close, sos_nfs_read, sos_nfs_write);

        if (sos_nfs_open(&vnode->fh, path, mode)) { return -1; }

        sos_stat_t stat_buf;
        if (sos_nfs_stat(path, &stat_buf)) { return -1; }
        //printf("%s has file size %lu\n", path, stat_buf.st_size);

        return fdt_insert(table, path, stat_buf.st_size, vnode, mode);
    }
}

int vfs_close(fd_table_t *table, int file)
{
    if (!fdt_entry_exists(table, file)) { return -1; }

    if (table->entries[file]->vnode->close_f(table->entries[file]->vnode->fh) == 0) {
        return fdt_delete(table, file);
    }
    return -1;
}

int vfs_getdirent(int pos, char *name, size_t nbyte)
{
    return sos_nfs_getdirent(pos, name, nbyte);
}

int vfs_stat(const char *path, sos_stat_t *buf)
{
    return sos_nfs_stat(path, buf);
}

int vfs_write(fd_table_t *table, int file, const char *buf, size_t nbyte)
{
    if (!fdt_entry_exists(table, file)) { return -1; }
    fd_entry_t *e = table->entries[file];

    struct nfsfh *fh = e->vnode->fh;
    size_t offset = e->offset;

    int written = e->vnode->write_f(fh, buf, offset, nbyte);
    e->offset += written;
    return written;
}

int vfs_read(fd_table_t *table, int file, char *buf, size_t nbyte)
{
    if (!fdt_entry_exists(table, file)) { return -1; }
    fd_entry_t *e = table->entries[file];

    struct nfsfh *fh = e->vnode->fh;
    size_t file_size = e->size;
    size_t offset = e->offset;

    if (offset >= file_size) { return 0; }
    if (file_size - offset < nbyte) { nbyte = file_size - offset; }

    int read = e->vnode->read_f(fh, buf, offset, nbyte);
    e->offset += read;
    return read;
}
