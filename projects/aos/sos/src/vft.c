
#include <fcntl.h>
#include <picoro/picoro.h>

#include "vft.h"
#include "syscall.h"
#include "utils/idalloc.h"
#include "frame_table.h"
#include "paging.h"
#include "vmem_layout.h"

#define MAX_FRAMES 2000
#define TMP 0x600000000000

static id_table_t *idt = NULL;
static frame_ref_t vframes[MAX_FRAMES];
static seL4_Word framecaps[MAX_FRAMES];

typedef struct kn {
    frame_ref_t frame_ref;
    seL4_Word frame_cap;
    vframe_ref_t vframe_ref;
    char reference;
} kn_t;

static unsigned long int used = 0;

static kn_t frames[CONFIG_SOS_FRAME_LIMIT-1];

static unsigned long int ptr = 0;

int vft_set_reference(vframe_ref_t vframe, seL4_CPtr vspace, seL4_Word vaddr)
{
    int found = 0;
    for (int i = 0; i < CONFIG_SOS_FRAME_LIMIT-1; i++) {
        if (frames[i].vframe_ref == vframe) {
            frames[i].reference = 1;
            found = 1;
        }
    }
    if (!found && vaddr != PROCESS_IPC_BUFFER) { return 0; }

    //printf("remapping %u . %p\n", vframe, framecaps[vframe]);
    seL4_CPtr frame_cap = cspace_alloc_slot(get_global_cspace());
    seL4_Error err = cspace_copy(get_global_cspace(), frame_cap, get_global_cspace(),
                    frame_page(vframes[vframe]), seL4_AllRights);
    assert(err == 0);
    //assert(frame_cap != seL4_CapNull);
    //printf("new %p\n", frame_cap);
    framecaps[vframe] = frame_cap;
    err = seL4_ARM_Page_Map(frame_cap, vspace, vaddr, seL4_AllRights,
            seL4_ARM_Default_VMAttributes);
    // seL4_Error err = seL4_ARM_Page_Remap(framecaps[vframe], vspace, seL4_ReadWrite,
    //         seL4_ARM_Default_VMAttributes | seL4_ARM_ExecuteNever);
    //printf("err %d\n", err);
    assert(err == 0);

    return 1;
}

int find_victim()
{
    while (1) {
        if (ptr == CONFIG_SOS_FRAME_LIMIT-1) { ptr = 0; }

        vframe_ref_t vframe = frames[ptr].vframe_ref;
        assert(framecaps[vframe] != 0);
       // printf("fc %p\n", framecaps[vframe]);
        if (frames[ptr].reference == 1) {
            frames[ptr].reference = 0;
            seL4_ARM_Page_Unmap(framecaps[vframe]);
            cspace_delete(get_global_cspace(), framecaps[vframe]);
            cspace_free_slot(get_global_cspace(), framecaps[vframe]);
            //printf("ptr %d had chance\n", ptr);
        }
        else {
            //printf("ptr %d victim\n", ptr);
            return ptr;
        }
        ptr++;
    }
}

void print_frames()
{
    for (int i = 0; i < CONFIG_SOS_FRAME_LIMIT-1; i++) {
        printf("%d: %u, %u\n", i, frames[i].frame_ref, frames[i].vframe_ref);
    }
}

void vft_page_out(int pl)
{
    frame_ref_t fr = frames[pl].frame_ref;
    unsigned long pos = frames[pl].vframe_ref;

    seL4_CPtr local = cspace_alloc_slot(get_global_cspace());
    seL4_Error err = cspace_copy(get_global_cspace(), local, get_global_cspace(), frame_page(fr), seL4_AllRights);
    assert(err == 0);
    err = map_frame(get_global_cspace(), local, seL4_CapInitThreadVSpace,
                    TMP, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    assert(err == 0);
    //printf("3\n");
    pagefile_write(TMP, pos);
    //printf("4\n");
        
    seL4_ARM_Page_Unmap(local);
    cspace_delete(get_global_cspace(), local);
    cspace_free_slot(get_global_cspace(), local);

    //memset(frame_data(fr), 0, 1<<12);
    //printf("paged out frame %d(%u) to %d\n", pos, fr, pos);
}

void vft_page_in(int pl, vframe_ref_t vframe)
{
    frame_ref_t fr = frames[pl].frame_ref;

    seL4_CPtr local = cspace_alloc_slot(get_global_cspace());
    seL4_Error err = cspace_copy(get_global_cspace(), local, get_global_cspace(), frame_page(fr), seL4_AllRights);
    assert(err == 0);
    err = map_frame(get_global_cspace(), local, seL4_CapInitThreadVSpace,
                    TMP, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    assert(err == 0);
    //printf("3\n");
    //printf("vframe is %u\n", vframe);
    pagefile_read(TMP, vframe);
    //printf("reading finished\n");
    //printf(":%s\n", 0x6000000002b0);

    seL4_ARM_Page_Unmap(local);
    cspace_delete(get_global_cspace(), local);
    cspace_free_slot(get_global_cspace(), local);

    frames[pl].vframe_ref = vframe;
    vframes[vframe] = fr;
}

frame_ref_t frame_ref_from_v(vframe_ref_t vframe)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) { return (frame_ref_t)vframe; }

    for (int i = 0; i < CONFIG_SOS_FRAME_LIMIT-1; i++) {
        if (frames[i].vframe_ref == vframe) {
            return frames[i].frame_ref;
        }
    }
//printf("vframe %u not found\n", vframe);
    vft_page_out(0);

    vft_page_in(0, vframe);

//printf("giving %u\n", frames[0].frame_ref);
    return frames[0].frame_ref;
}

vframe_ref_t valloc_frame()
{
    if (idt == NULL) { idt = id_table_init(0, 1, MAX_FRAMES); }

    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return (vframe_ref_t)alloc_frame();
    } else if (used < CONFIG_SOS_FRAME_LIMIT-1) {

        //printf("allocating frame at index %u\n", used);
        frame_ref_t nf = alloc_frame();

        int i = id_alloc(idt, 1);
        vframes[i] = nf;

        kn_t k = { nf, frame_page(nf), (vframe_ref_t)i, 1 };
        frames[used] = k;

        used++;
        return i;

    } else {
        int v = find_victim();
        //printf("paging out one frame\n");
        vft_page_out(v);

        int i = id_alloc(idt, 1);
        vframes[i] = frames[v].frame_ref;
        frames[v].vframe_ref = i;
        return i;
    }
}

void vframe_add_cap(vframe_ref_t vframe, seL4_Word frame_cap)
{
    framecaps[vframe] = frame_cap;
}

void print_limit()
{
    //printf("limit %d\n", CONFIG_SOS_FRAME_LIMIT);
}
