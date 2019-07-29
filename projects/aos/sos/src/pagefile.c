#include <picoro/picoro.h>
#include <stdio.h>

#include "pagefile.h"
#include "fs/sos_nfs.h"
#include "fs/fs_types.h"

static int pf_ready = 0;

static struct nfsfh *fh = NULL;

void *pagefile_init(void *p)
{
    sos_nfs_open(&fh, "pagefile", 2);
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
    //sos_nfs_lseek(fh, offset);
    sos_nfs_write(fh, buf, offset, 1<<12);
}

void pagefile_read(char *buf, unsigned long pos)
{
    size_t offset = pos * (1<<12);
    //printf("pagefile_reading from %u\n", offset);
    //sos_nfs_lseek(fh, offset);
    sos_nfs_read(fh, buf, offset, 1<<12);
    //printf("read done\n");
    //printf("{%s}\n", buf);
}
