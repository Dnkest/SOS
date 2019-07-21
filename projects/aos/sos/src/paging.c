#include <picoro/picoro.h>

#include "paging.h"
#include "fs/sos_nfs.h"
#include "fs/fs_types.h"

static int pf_ready = 0;

struct nfs_d {
    int err;
    void *data;
    int done;
};

static struct nfsfh *fh = NULL;

void pagefile_test();

void nfs_cb1(int err, struct nfs_context *nfs, void *data,
                       void *private_data)
{
    struct nfs_d *nfs_d = private_data;
    nfs_d->data = data;
    nfs_d->err = err;
    nfs_d->done = 1;
}

void *pagefile_init(void *p)
{
    //printf("here\n");
    while (!sos_nfs_get_context()) { yield(0); }
//printf("p1\n");
    // struct nfs_d d0 = { .data = NULL, .err = 0, .done = 0};
    // nfs_opendir_async(sos_nfs_get_context(), "", nfs_cb1, &d0);
    // while (!d0.done) {
    //     yield(0);
    // }
//printf("p2\n");
    struct nfs_d d = { .data = NULL, .err = 0, .done = 0};
    nfs_creat_async(sos_nfs_get_context(), "pagefile", 0666, nfs_cb1, &d);
    while (!d.done) { yield(0); }
//printf("p3\n");

    struct nfs_d d1 = { .data = NULL, .err = 0, .done = 0};
    nfs_open_async(sos_nfs_get_context(), "pagefile", 2, nfs_cb1, &d1);
    while (!d1.done) { yield(0); }
    
    fh = (struct nfsfh *)d1.data;
    //printf("fh %p\n", fh);


//printf("pdone\n");
//pagefile_test();
    pf_ready = 1;
    return NULL;
}

int pagefile_ready()
{
    return pf_ready;
}

void pagefile_write(char *buf, unsigned long pos)
{
    size_t offset = pos * (1<<12);
    //printf("pagefile_writing to %u\n", offset);
    sos_nfs_lseek(fh, offset);
    sos_nfs_write(fh, buf, 0, 1<<12);
}

void pagefile_read(char *buf, unsigned long pos)
{
    size_t offset = pos * (1<<12);
    //printf("pagefile_reading from %u\n", offset);
    sos_nfs_lseek(fh, offset);
    sos_nfs_read(fh, buf, 0, 1<<12);
}

void pagefile_test()
{
    pagefile_write("hahaha", 0);

    char *buf;
    pagefile_read(buf, 0);
    printf("{%s}\n", buf);
}
