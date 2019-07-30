
#include <fcntl.h>
#include <picoro/picoro.h>

#include "vft.h"
#include "syscall.h"
#include "utils/low_avail_id.h"
#include "frame_table.h"
#include "pagefile.h"
#include "vmem_layout.h"
#include "mapping.h"

#define MAX_VFRAMES 4000

#if CONFIG_SOS_FRAME_LIMIT == 0
    #define MAX_PFRAMES 1
#else
    #define MAX_PFRAMES CONFIG_SOS_FRAME_LIMIT-1
#endif

static low_avail_id_t *vframe_ids = NULL;
static low_avail_id_t *paging_ids = NULL;

typedef struct vframe_ent {
    frame_ref_t frame_ref;
    seL4_CPtr frame_cap;
    seL4_Word vaddr;
    seL4_CPtr vspace;
} vframe_ent_t;

static vframe_ent_t vframes[MAX_VFRAMES];

typedef struct pframe_ent {
    frame_ref_t frame_ref;
    vframe_ref_t vframe_ref;
    char reference;
    char pin;
} pframe_ent_t;

static pframe_ent_t pframes[MAX_PFRAMES];
static unsigned long int used = 0;
static unsigned long int ptr = 0;

int find_victim()
{
    while (1) {
        if (ptr == MAX_PFRAMES) { ptr = 0; }

        if (pframes[ptr].reference == 1 && !pframes[ptr].pin) {
            pframes[ptr].reference = 0;
            seL4_Error err = cspace_revoke(global_cspace(), frame_page(pframes[ptr].frame_ref));
            assert(err == 0);
            ptr++;
        }
        else if (!pframes[ptr].pin) {
            return ptr++;
        } else {
            ptr++;
        }
    }
}

void print_frames()
{
    for (int i = 0; i < MAX_PFRAMES; i++) {
        printf("%d: %u, %u\n", i, pframes[i].frame_ref, pframes[i].vframe_ref);
    }
}

void vft_page_out(int v)
{
    frame_ref_t fr = pframes[v].frame_ref;
    unsigned long pos = pframes[v].vframe_ref;
    seL4_Word tmp = low_avail_id_alloc(paging_ids, 1);

    seL4_CPtr local = cspace_alloc_slot(global_cspace());
    assert(local != seL4_CapNull);
    seL4_Error err = cspace_copy(global_cspace(), local, global_cspace(), frame_page(fr), seL4_AllRights);
    assert(err == 0);
    err = map_frame(global_cspace(), local, seL4_CapInitThreadVSpace,
                    tmp, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    assert(err == 0);

    pagefile_write(tmp, pos);
    
    memset(tmp, 0, 1<<12);
    seL4_ARM_Page_Unmap(local);
    cspace_delete(global_cspace(), local);
    cspace_free_slot(global_cspace(), local);

    err = cspace_revoke(global_cspace(), frame_page(fr));
    assert(err == 0);
    low_avail_id_free(paging_ids, tmp, 1);
    //printf("paging out %p\n", vframes[pframes[v].vframe_ref].vaddr);
    //pframes[v].reference = 1;
    //printf("paged out frame %d(%u) to %d\n", pos, fr, pos);
}

void vft_page_in(int v, vframe_ref_t vframe)
{
    frame_ref_t fr = pframes[v].frame_ref;
    unsigned long pos = vframe;
    seL4_Word tmp = low_avail_id_alloc(paging_ids, 1);

    seL4_CPtr local = cspace_alloc_slot(global_cspace());
    assert(local != seL4_CapNull);
    seL4_Error err = cspace_copy(global_cspace(), local, global_cspace(), frame_page(fr), seL4_AllRights);
    assert(err == 0);
    err = map_frame(global_cspace(), local, seL4_CapInitThreadVSpace,
                    tmp, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    assert(err == 0);
    //printf("3\n");
    //printf("vframe is %u\n", vframe);
    pagefile_read(tmp, pos);
    //printf("reading finished\n");
    //printf(":%s\n", 0x6000000002b0);

    seL4_ARM_Page_Unmap(local);
    cspace_delete(global_cspace(), local);
    cspace_free_slot(global_cspace(), local);

    vframe_ent_t e = vframes[vframe];
    err = cspace_copy(global_cspace(), e.frame_cap, global_cspace(), frame_page(fr), seL4_AllRights);
    assert(err == 0);
    err = seL4_ARM_Page_Map(e.frame_cap, e.vspace, e.vaddr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    low_avail_id_free(paging_ids, tmp, 1);
    //printf("paging in %p\n", vframes[vframe].vaddr);
}

frame_ref_t frame_ref_from_v(vframe_ref_t vframe)
{
    //printf("frame from v called\n");
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) { return (frame_ref_t)vframe; }

    for (int i = 0; i < MAX_PFRAMES; i++) {
        pframe_ent_t pe = pframes[i];
        if (pe.vframe_ref == vframe) {
            if (pe.reference == 0) {
                //printf("vframe %u was 0", vframe);
                vframe_ent_t e = vframes[vframe];
                seL4_Error err = cspace_copy(global_cspace(), e.frame_cap, global_cspace(), frame_page(pe.frame_ref), seL4_AllRights);
                assert(err == 0);
                //printf(", cap %p\n", e.frame_cap);
                err = seL4_ARM_Page_Map(e.frame_cap, e.vspace, e.vaddr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
                assert(err == 0);
                pframes[i].reference = 1;
            }
            return pframes[i].frame_ref;
        }
    }
    //printf("vframe %u not found\n", vframe);
    int v = find_victim();
    vft_page_out(v);

    vft_page_in(v, vframe);
    pframes[v].vframe_ref = vframe;
    pframes[v].reference = 1;

    vframes[vframe].frame_ref = pframes[v].frame_ref;

    //printf("giving %u\n",pframes[v].frame_ref );
    return pframes[v].frame_ref;
}

vframe_ref_t valloc_frame()
{
    if (vframe_ids == NULL) { vframe_ids = id_table_init(1, 1, MAX_VFRAMES); }
    if (paging_ids == NULL) { paging_ids = id_table_init(SOS_PAGING, 4096, 100); }

    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return (vframe_ref_t)alloc_frame();
    } else if (used < MAX_PFRAMES) {

        //printf("allocating frame at index %u\n", used);
        frame_ref_t nf = alloc_frame();

        int i = low_avail_id_alloc(vframe_ids, 1);
        vframes[i].frame_ref = nf;

        pframe_ent_t e = { nf, (vframe_ref_t)i, 1, 0 };
        pframes[used] = e;

        used++;
        
        // if (used == MAX_PFRAMES) {
        //     for (int i = 0; i < MAX_PFRAMES; i++) {
        //         printf("%u, %u\n", pframes[i].frame_ref, pframes[i].vframe_ref);
                
        //     }
        // }
        return i;
    } else {
        int v = find_victim();
        //printf("paging out one frame\n");
        vft_page_out(v);

        int i = low_avail_id_alloc(vframe_ids, 1);
        vframes[i].frame_ref = pframes[v].frame_ref;

        pframes[v].vframe_ref = i;
        pframes[v].reference = 1;
        return i;
    }
}

void vframe_add_cap(vframe_ref_t vframe, seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace)
{
    vframes[vframe].frame_cap = frame_cap;
    vframes[vframe].vaddr = vaddr;
    vframes[vframe].vspace = vspace;
}

void print_limit()
{
    //printf("limit %d\n", CONFIG_SOS_FRAME_LIMIT);
}

vframe_ref_t vframe_dup(vframe_ref_t vframe)
{
    vframe_ent_t origin = vframes[vframe];

    int i = low_avail_id_alloc(vframe_ids, 1);
    vframes[i].frame_ref = origin.frame_ref;

    return i;
}

void vft_pin_frame(frame_ref_t frame_num)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return;
    }
    int i = 0;
    while (pframes[i].frame_ref != frame_num) { i++; }
    if (i == MAX_PFRAMES) { return; }
    pframes[i].pin = 1;
}

void vft_unpin_frame(frame_ref_t frame_num)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return;
    }

    int i = 0;
    while (pframes[i].frame_ref != frame_num) { i++; }
    if (i == MAX_PFRAMES) { return; }
    pframes[i].pin = 0;
}
