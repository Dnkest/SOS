#include <picoro/picoro.h>
#include <utils/page.h>
#include <stdio.h>

#include "paging.h"
#include "utils/kmalloc.h"
#include "utils/circular_id.h"
#include "vmem_layout.h"
#include "globals.h"
#include "fs/sos_nfs.h"
#include "fs/fs_types.h"

static struct nfsfh *fh = NULL;
static int pready = 0;

static circular_id_t *paging_addr_table;

static char lock = 0;

void *paging_init(void *p)
{
    sos_nfs_open(&fh, "pagefile", 2);
    paging_addr_table = circular_id_init(SOS_PAGING, PAGE_SIZE_4K, 10);
    pready = 1;
}

void page_out(const char *buf, unsigned long pos)
{

}

void page_in(char *buf, unsigned long pos)
{

}

int paging_ready()
{
    return pready;
}
