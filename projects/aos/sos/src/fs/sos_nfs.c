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

int sos_nfs_open(struct nfsfh **fh, const char *path, int flags)
{
    while (!nfs) { yield(0); }
    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    int ret;
    if (sos_nfs_stat(path, 0) == -1) {
        ret = nfs_creat_async(nfs, path, 0666, nfs_callback, &nfs_d);
    } else {
        ret = nfs_open_async(nfs, path, flags, nfs_callback, &nfs_d);
    }
    while (!nfs_d.done) { yield(0); }
    *fh = (struct nfsfh *)nfs_d.data;
    return ret;
}

int sos_nfs_close(struct nfsfh *fh)
{
    while (!nfs) { yield(0); }

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    
    if (nfs_close_async(nfs, fh, nfs_callback, &nfs_d) != 0) { return -1; }
    while (!nfs_d.done) { yield(0); }

    return nfs_d.err;
}

int sos_nfs_read(struct nfsfh *fh, char *buf, size_t offset, size_t nbyte)
{
    while (!nfs) { yield(0); }

    nfs_read_data_t nfs_d = { .buf = buf, .err = 0, .done = 0, .nbyte = nbyte };
    
    if (nfs_pread_async(nfs, fh, offset, nbyte, nfs_read_callback, &nfs_d)) {
        return -1;
    }
    while (!nfs_d.done) { yield(0); }

    return nbyte;
}

int sos_nfs_write(struct nfsfh *fh, const char *buf, size_t offset, size_t nbyte)
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
    int ret;
    if (entry == NULL) {
        ret = -1;
    } else if (buf != NULL) {
        buf->st_type = entry->type;
        buf->st_size = entry->size;
        buf->st_fmode = entry->mode;
        buf->st_atime = entry->atime_nsec/1000000;
        buf->st_ctime = entry->ctime_nsec/1000000;
        ret = 0;
    } else {
        ret = 0;
    }

    nfs_closedir(nfs, dir);
    return ret;
}

void sos_nfs_set_context(struct nfs_context *context)
{
    nfs = context;
}

struct nfs_context *sos_nfs_get_context()
{
    return nfs;
}

void sos_nfs_lseek(struct nfsfh *fh, size_t offset)
{
    while (!nfs) {}

    nfs_data_t nfs_d = { .data = NULL, .err = 0, .done = 0};
    
    nfs_lseek_async(nfs, fh, offset, SEEK_SET, nfs_callback, &nfs_d);

    while (!nfs_d.done) {}
}
