#include <fcntl.h>
#include <picoro/picoro.h>
#include <stdlib.h>
#include <stdio.h>

#include "sos_nfs.h"

static struct nfs_context *nfs = NULL;

typedef struct nfs_data {
    int err;
    void *data;
    int done;
} nfs_data_t;

typedef struct nfs_read_data {
    int err;
    char *buf;
    size_t nbyte;
    int done;
} nfs_read_data_t;

void nfs_callback(int err, struct nfs_context *nfs, void *data,
                       void *private_data)
{
    nfs_data_t *nfs_d = private_data;
    nfs_d->data = data;
    nfs_d->err = err;
    nfs_d->done = 1;
}

void nfs_read_callback(int err, struct nfs_context *nfs, void *data,
                       void *private_data)
{
    nfs_read_data_t *nfs_d = private_data;
    //printf("copying %lu bytes\n", nfs_d->nbyte);
    memcpy(nfs_d->buf, data, nfs_d->nbyte);
    nfs_d->err = err;
    nfs_d->done = 1;
}

int sos_nfs_open(fd_table_t *table, const char *path, fmode_t mode)
{
    while (!nfs) { //printf(" not ready yet ?\n");
    yield(0); }
//printf(" ready !\n");
    vnode_t *vnode = vnode_init(sos_nfs_close, sos_nfs_read, sos_nfs_write);

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    if (sos_nfs_stat(path, 0) == -1) {
        nfs_creat_async(nfs, path, 0666, nfs_callback, &nfs_d);
    } else {
        nfs_open_async(nfs, path, mode, nfs_callback, &nfs_d);
    }
    while (!nfs_d.done) { yield(0); }
    vnode->fh = (struct nfsfh *)nfs_d.data;

    sos_stat_t stat_buf;
    sos_nfs_stat(path, &stat_buf);
    printf("%s has file size %lu\n", path, stat_buf.st_size);
    return fdt_insert(table, path, stat_buf.st_size, vnode, mode);
}

int sos_nfs_close(struct nfsfh *fh)
{
    while (!nfs) { yield(0); }

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    
    nfs_close_async(nfs, fh, nfs_callback, &nfs_d);
    while (!nfs_d.done) { yield(0); }

    return nfs_d.err;
}

int sos_nfs_read(struct nfsfh *fh, char *buf, size_t offset, size_t nbyte)
{
    while (!nfs) { yield(0); }

    nfs_read_data_t nfs_d = { .buf = buf, .err = 0, .done = 0, .nbyte = nbyte };
    
    nfs_pread_async(nfs, fh, offset, nbyte, nfs_read_callback, &nfs_d);
    while (!nfs_d.done) { yield(0); }

    return nbyte;
}

int sos_nfs_write(struct nfsfh *fh, char *buf, size_t offset, size_t nbyte)
{
    while (!nfs) { yield(0); }

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    
    nfs_pwrite_async(nfs, fh, offset, nbyte, buf, nfs_callback, &nfs_d);
    while (!nfs_d.done) { yield(0); }
    //printf("write %d\n", nfs_d.data);
    return nbyte;
}

int sos_nfs_getdirent(int pos, char *name, size_t nbyte)
{
    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    nfs_opendir_async(nfs, "", nfs_callback, &nfs_d);
    while (!nfs_d.done) { yield(0); }

    struct nfsdir *dir = nfs_d.data;

    struct nfsdirent *entry = nfs_readdir(nfs, dir);
    int i;
    for (i = 0; i < pos && entry != NULL && entry->next != NULL; i++) {
        entry = entry->next;
    }
    if (i < pos) {
        if (i == pos-1) {
            return 0;
        }
        return -1;
    }
    memcpy(name, entry->name, nbyte);

    nfs_closedir(nfs, dir);
    size_t ret = (strlen(name) > nbyte) ? nbyte : strlen(name);
    return ret;
}

int sos_nfs_stat(const char *path, sos_stat_t *buf)
{
    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    nfs_opendir_async(nfs, "", nfs_callback, &nfs_d);
    while (!nfs_d.done) {
        yield(0);
    }

    struct nfsdir *dir = nfs_d.data;

    struct nfsdirent *entry = nfs_readdir(nfs, dir);
    while (entry != NULL && strcmp(entry->name, path) != 0) {
        entry = entry->next;
    }
    int vnode;
    if (entry == NULL) {
        vnode = -1;
    } else if (buf != NULL) {
        buf->st_type = entry->type;
        buf->st_size = entry->size;
        buf->st_fmode = entry->mode;
        buf->st_atime = entry->atime_nsec/1000000;
        buf->st_ctime = entry->ctime_nsec/1000000;
        vnode = 0;
    } else {
        vnode = 0;
    }

    nfs_closedir(nfs, dir);
    return vnode;
}

void sos_nfs_set_context(struct nfs_context *context)
{
    nfs = context;
}

struct nfs_context *sos_nfs_get_context()
{
    return nfs;
}

int sos_nfs_open_b(fd_table_t *table, const char *path, fmode_t mode)
{
    if (!nfs) { printf("not ready\n"); return 0; }

    
    vnode_t *vnode = vnode_init(sos_nfs_close, sos_nfs_read, sos_nfs_write);

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    nfs_creat_async(nfs, path, 0666, nfs_callback, &nfs_d);
    while (!nfs_d.done) {}

    nfs_data_t nfs_d2 = { .data = NULL, .err = 0, .done = 0};
    nfs_open_async(nfs, path, mode, nfs_callback, &nfs_d);
    while (!nfs_d2.done) {}
    
    vnode->fh = (struct nfsfh *)nfs_d.data;

    return fdt_insert(table, path, 0xfffffffff, vnode, mode);
}

int sos_nfs_read_b(struct nfsfh *fh, char *buf, size_t offset, size_t nbyte)
{
    while (!nfs) {}

    nfs_read_data_t nfs_d = { .buf = buf, .err = 0, .done = 0, .nbyte = nbyte };
    
    nfs_pread_async(nfs, fh, offset, nbyte, nfs_read_callback, &nfs_d);
    while (!nfs_d.done) {}

    return nbyte;
}

int sos_nfs_write_b(struct nfsfh *fh, char *buf, size_t offset, size_t nbyte)
{
    while (!nfs) {}

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    
    nfs_pwrite_async(nfs, fh, offset, nbyte, buf, nfs_callback, &nfs_d);
    while (!nfs_d.done) {}
    //printf("write %d\n", nfs_d.data);
    return nbyte;
}

void sos_nfs_lseek(struct nfsfh *fh, size_t offset)
{
    while (!nfs) {}

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    
    nfs_lseek_async(nfs, fh, offset, SEEK_SET, nfs_callback, &nfs_d);

    while (!nfs_d.done) {}
}
