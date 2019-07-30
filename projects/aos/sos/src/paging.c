#include <picoro/picoro.h>
#include <utils/page.h>
#include <stdio.h>
#include <cspace/cspace.h>

#include "mapping.h"
#include "paging.h"
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
    return NULL;
}

void page_out(frame_ref_t frame, unsigned long pos)
{
    while (lock == 1) { yield(0); }
    lock = 1;

    seL4_Word tmp = circular_id_alloc(paging_addr_table, 1);

    seL4_CPtr local = cspace_alloc_slot(global_cspace());
    assert(local != seL4_CapNull);
    seL4_Error err = cspace_copy(global_cspace(), local, global_cspace(), frame_page(frame), seL4_AllRights);
    assert(err == 0);
    err = map_frame(global_cspace(), local, global_vspace(),
                    tmp, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    assert(err == 0);

    sos_nfs_write(fh, (const char *)tmp, pos * (1<<12), 1<<12);

    seL4_ARM_Page_Unmap(local);
    cspace_delete(global_cspace(), local);
    cspace_free_slot(global_cspace(), local);

    circular_id_free(paging_addr_table, tmp, 1);
    lock = 0;
}

void page_in(frame_ref_t frame, unsigned long pos)
{
    while (lock == 1) { yield(0); }
    lock = 1;

    seL4_Word tmp = circular_id_alloc(paging_addr_table, 1);

    seL4_CPtr local = cspace_alloc_slot(global_cspace());
    assert(local != seL4_CapNull);
    seL4_Error err = cspace_copy(global_cspace(), local, global_cspace(), frame_page(frame), seL4_AllRights);
    assert(err == 0);
    err = map_frame(global_cspace(), local, global_vspace(),
                    tmp, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    assert(err == 0);

    sos_nfs_read(fh, (char *)tmp, pos * (1<<12), 1<<12);

    seL4_ARM_Page_Unmap(local);
    cspace_delete(global_cspace(), local);
    cspace_free_slot(global_cspace(), local);

    circular_id_free(paging_addr_table, tmp, 1);
    lock = 0;
}

int paging_ready()
{
    return pready;
}
