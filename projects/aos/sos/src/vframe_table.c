
#include <cspace/cspace.h>
#include <picoro/picoro.h>
#include <stdio.h>

#include "paging.h"
#include "utils/kmalloc.h"
#include "vframe_table.h"

#define MAX_VFRAMES 2000000

#if CONFIG_SOS_FRAME_LIMIT == 0
    #define MAX_PFRAMES 1
#else
    #define MAX_PFRAMES CONFIG_SOS_FRAME_LIMIT-1
#endif

struct pframe {
    frame_ref_t frame_ref;
    vframe_ref_t vframe_ref;
    char reference;
    char pin;
};
static struct pframe pframes[MAX_PFRAMES];
static unsigned int used = 0;
static unsigned long int clock_ptr = 0;

struct vframe {
    vframe_ref_t id;
    frame_ref_t frame_ref;
    seL4_CPtr frame_cap;
    seL4_Word vaddr;
    seL4_CPtr vspace;

    struct vframe *next;
};

static struct {
    struct vframe *head;
    struct vframe *tail;
} vframe_table = {
    .head = NULL,
    .tail = NULL,
};

/* priodically jump back to list head to avoid leaking free slots. */
#define JMP_CYCLE 10
static unsigned int jmp_cur = 0;

int jump_back()
{
    if (++jmp_cur > 10) {
        jmp_cur = 0;
        return 1;
    }
    return 0;
}

vframe_ref_t vframe_table_insert(frame_ref_t frame, seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace);
struct vframe *vframe_table_find(vframe_ref_t vframe);

static void page_out_frame(int n);
static void page_in_frame(int n, vframe_ref_t vframe);

static int find_victim()
{
    while (1) {
        if (clock_ptr == MAX_PFRAMES) { clock_ptr = 0; }

        if (pframes[clock_ptr].reference == 1 && !pframes[clock_ptr].pin) {
            pframes[clock_ptr].reference = 0;
            seL4_Error err = cspace_revoke(global_cspace(), frame_page(pframes[clock_ptr].frame_ref));
            assert(err == 0);
            clock_ptr++;
        }
        else if (!pframes[clock_ptr].pin) {
            return clock_ptr++;
        } else {
            clock_ptr++;
        }
    }
}

vframe_ref_t alloc_vframe(seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace)
{
    vframe_ref_t vref = 0;
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return (vframe_ref_t)alloc_frame();
    } else if (used < MAX_PFRAMES) {
        //printf("allocating frame at index %u\n", used);

        frame_ref_t pref = alloc_frame();
        vref = vframe_table_insert(pref, frame_cap, vaddr, vspace);
        struct pframe p = { pref, vref, 1, 0 };
        pframes[used] = p;
        used++;
        return vref;
    } else {
        //printf("paging out one frame\n");

        int v = find_victim();
        page_out_frame(v);
        vref = vframe_table_insert(pframes[v].frame_ref, frame_cap, vaddr, vspace);
        pframes[v].vframe_ref = vref;
        pframes[v].reference = 1;
        return vref;
    }
}

frame_ref_t frame_from_vframe(vframe_ref_t vframe)
{
    //printf("frame from v called %u\n", vframe);
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) { return (frame_ref_t)vframe; }

    struct vframe *v = vframe_table_find(vframe);
    for (int i = 0; i < MAX_PFRAMES; i++) {
        struct pframe p = pframes[i];
        if (p.vframe_ref == vframe) {
            if (p.reference == 0) {
                assert(v != NULL);
                seL4_Error err = cspace_copy(global_cspace(), v->frame_cap, global_cspace(), frame_page(p.frame_ref), seL4_AllRights);
                assert(err == 0);
                err = seL4_ARM_Page_Map(v->frame_cap, v->vspace, v->vaddr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
                assert(err == 0);
                pframes[i].reference = 1;
            }
            return pframes[i].frame_ref;
        }
    }
    //printf("vframe %u not found\n", vframe);
    int k = find_victim();
    page_out_frame(k);

    page_in_frame(k, vframe);
    pframes[k].vframe_ref = vframe;
    pframes[k].reference = 1;

    v->frame_ref = pframes[k].frame_ref;
    return pframes[k].frame_ref;
}

vframe_ref_t vframe_table_insert(frame_ref_t frame, seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace)
{
    struct vframe *v = kmalloc(sizeof(struct vframe));
    v->frame_ref = frame;
    v->frame_cap = frame_cap;
    v->vaddr = vaddr;
    v->vspace = vspace;
    if (vframe_table.head == NULL) {
        vframe_table.head = vframe_table.tail = v;
        v->id = 1;
    } else {
        if (!jump_back()) {
            v->id = vframe_table.tail->id + 1;
            vframe_table.tail->next = v;
            vframe_table.tail = v;
        } else {
            struct vframe *cur = vframe_table.head, *prev = NULL;
            while (cur != NULL) {
                prev = cur;
                cur = cur->next;
                if (cur == NULL) {
                    v->id = prev->id + 1;
                    prev->next = v;
                    vframe_table.tail = v;
                } else if (cur->id != prev->id + 1) {
                    v->id = prev->id + 1;
                    prev->next = v;
                    v->next = cur;
                }
            }
        }
    }
    return v->id;
}

struct vframe *vframe_table_find(vframe_ref_t vframe)
{
    struct vframe *cur = vframe_table.head;
    while (cur != NULL) {
        if (cur->id == vframe) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static void page_out_frame(int n)
{
    frame_ref_t fr = pframes[n].frame_ref;
    unsigned long pos = pframes[n].vframe_ref;
    page_out(fr, pos);

    seL4_Error err = cspace_revoke(global_cspace(), frame_page(fr));
    assert(err == 0);
}

static void page_in_frame(int n, vframe_ref_t vframe)
{
    frame_ref_t fr = pframes[n].frame_ref;
    unsigned long pos = vframe;
    page_in(fr, pos);
    
    struct vframe *v = vframe_table_find(vframe);
    assert(v != NULL);
    seL4_Error err = cspace_copy(global_cspace(), v->frame_cap, global_cspace(),
                                frame_page(fr), seL4_AllRights);
    assert(err == 0);
    err = seL4_ARM_Page_Map(v->frame_cap, v->vspace, v->vaddr, seL4_AllRights,
                                seL4_ARM_Default_VMAttributes);
    assert(err == 0);
}

void vframe_pin(frame_ref_t frame_num)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return;
    }
    int i = 0;
    while (pframes[i].frame_ref != frame_num) { i++; }
    if (i == MAX_PFRAMES) { return; }
    pframes[i].pin = 1;
}

void vframe_unpin(frame_ref_t frame_num)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return;
    }

    int i = 0;
    while (pframes[i].frame_ref != frame_num) { i++; }
    if (i == MAX_PFRAMES) { return; }
    pframes[i].pin = 0;
}
