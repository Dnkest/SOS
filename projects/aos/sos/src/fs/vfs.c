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
        printf("openning console\n");
        return serial_open(table, mode);
    } else {
        printf("openning %s\n", path);
        return sos_nfs_open(table, path, mode);
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
    if (strcmp(e->path, "console") == 0) {
        return e->vnode->write_f(0, buf, 0, nbyte);
    } else {
        struct nfsfh *fh = e->vnode->fh;
        //size_t file_size = e->size;
        size_t offset = e->offset;
        // printf("1writing %d, %s, %lu->%lu\n", file, buf, offset, nbyte);
        // nbyte = (offset >= file_size) ? 0 : file_size - offset;
        // printf("nbyte %lu\n", nbyte);
        // if (nbyte == 0) { return 0; }
        printf("writing %d, offset:%lu(%lu)\n", file, offset, nbyte);
        int written = e->vnode->write_f(fh, buf, offset, nbyte);
        e->offset += written;
        return written;
    }
}

int vfs_read(fd_table_t *table, int file, char *buf, size_t nbyte)
{
    if (!fdt_entry_exists(table, file)) { return -1; }
    fd_entry_t *e = table->entries[file];
    if (strcmp(e->path, "console") == 0 && file == 3) {
        return e->vnode->read_f(0, buf, 0, nbyte);
    } else {
        struct nfsfh *fh = e->vnode->fh;
        size_t file_size = e->size;
        size_t offset = e->offset;

        if (offset >= file_size) { return 0; }
        if (file_size - offset < nbyte) { nbyte = file_size - offset; }

        printf("readding %d, offset:%lu(%lu)\n", file, offset, nbyte);
        int read = e->vnode->read_f(fh, buf, offset, nbyte);
        e->offset += read;
        return read;
    }
}
