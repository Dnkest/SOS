
#include <cspace/cspace.h>
#include <picoro/picoro.h>
#include <sel4/sel4.h>
#include <stdio.h>
#include <stdlib.h>

#include "paging.h"
//#include "utils/malloc.h"
#include "vframe_table.h"

#if CONFIG_SOS_FRAME_LIMIT == 0
    #define MAX_PFRAMES 1
#else
    #define MAX_PFRAMES CONFIG_SOS_FRAME_LIMIT-1
#endif

typedef struct vframe {
    vframe_ref_t id;
    seL4_CPtr frame_cap;
    seL4_Word vaddr;
    seL4_CPtr vspace;

    struct vframe *next;
} vframe_t;

static struct {
    vframe_t *head;
    vframe_t *tail;
} vframe_table = {
    .head = NULL,
    .tail = NULL,
};

typedef struct pframe {
    frame_ref_t frame_ref;
    vframe_t *vframe;
    char reference;
    char pin;

    struct pframe *next;
} pframe_t;
static struct {
    pframe_t *head;
    pframe_t *tail;
} pframe_table = {
    .head = NULL,
    .tail = NULL,
};
static unsigned int used = 0;

static pframe_t *clock_ptr;

/* priodically jump back to list head to avoid leaking free slots. */
#define JMP_CYCLE 100
static unsigned int jmp_cur = 0;

int jump_back()
{
    // if (++jmp_cur > 10) {
    //     jmp_cur = 0;
    //     return 1;
    // }
    return 1;
}

vframe_t *vframe_table_insert(seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace);
vframe_t *vframe_table_find(vframe_ref_t vframe);

void pframe_table_pushback(frame_ref_t frame_ref, vframe_t *vframe);

static void page_out_frame(pframe_t *p);
static vframe_t *page_in_frame(pframe_t *p, vframe_ref_t vframe);

void free_vframe(vframe_ref_t vframe)
{
    //printf("removing vframe %u\n", vframe);
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        free_frame((frame_ref_t)vframe);
    } else {
        pframe_t *pc = pframe_table.head;
        vframe_t *v;
        while (pc != NULL) {
            v = pc->vframe;
            if (v->id == vframe) {
                pc->reference = 0;
                pc->vframe = vframe_table.head;
                clock_ptr = pc;
                break;
            }
            pc = pc->next;
        }
        vframe_t *cur = vframe_table.head, *prev = NULL;
        while (cur != NULL) {
            prev = cur;
            cur = cur->next;
            if (cur != NULL && cur->id == vframe) {
                cspace_delete(global_cspace(), cur->frame_cap);
                cspace_free_slot(global_cspace(), cur->frame_cap);
                prev->next = cur->next;
                free(cur);
                return;
            }
        }
    }
}

static pframe_t *find_victim()
{
    seL4_Error err;
    while (1) {
        if (clock_ptr == NULL) { clock_ptr = pframe_table.head; }

        if (!clock_ptr->pin) {
            if (clock_ptr->reference == 1) {
                clock_ptr->reference = 0;
                err = cspace_revoke(global_cspace(), frame_page(clock_ptr->frame_ref));
                assert(err == 0);
            } else {
                // pframe_t *ret = clock_ptr;
                // clock_ptr = clock_ptr->next;
                return clock_ptr;
            }
        }
        clock_ptr = clock_ptr->next;
    }
}

vframe_ref_t alloc_vframe(seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace)
{
    vframe_t *v;
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return (vframe_ref_t)alloc_frame();
    } else if (used < MAX_PFRAMES) {
        //printf("allocating frame at index %u\n", used);
        frame_ref_t pref = alloc_frame();

        v = vframe_table_insert(frame_cap, vaddr, vspace);
        pframe_table_pushback(pref, v);
        used++;
        //printf("new id %u\n", v->id);
        return v->id;
    } else {
        pframe_t *victim = find_victim();
        page_out_frame(victim);
        v = vframe_table_insert(frame_cap, vaddr, vspace);
        victim->vframe = v;
        victim->reference = 1;
        //printf("new id (page out) %u\n", v->id);
        return v->id;
    }
}

frame_ref_t frame_from_vframe(vframe_ref_t vframe)
{
    //printf("frame from v called %u\n", vframe);
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) { return (frame_ref_t)vframe; }

    seL4_Error err;
    vframe_t *v;
    pframe_t *cur = pframe_table.head;
    while (cur != NULL) {
        v = cur->vframe;
        if (v->id == vframe) {
            if (cur->reference == 0) {
                //printf("here\n");
                err = cspace_copy(global_cspace(), v->frame_cap, global_cspace(),
                                    frame_page(cur->frame_ref), seL4_AllRights);
                assert(err == 0);
                //if (err) printf("err1 %d\n", err);
                err = seL4_ARM_Page_Map(v->frame_cap, v->vspace, v->vaddr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
                assert(err == 0);
                //if (err) printf("err2 %d\n", err);
                cur->reference = 1;
            }
            return cur->frame_ref;
        }
        cur = cur->next;
    }
    //printf("vframe %u not found\n", vframe);
    pframe_t *victim = find_victim();
    //printf("paging out\n");
    page_out_frame(victim);
//printf("paging in\n");
    v = page_in_frame(victim, vframe);
    victim->vframe = v;
    victim->reference = 1;

    return victim->frame_ref;
}

vframe_t *vframe_table_insert(seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace)
{
    vframe_t *v = (vframe_t *)malloc(sizeof(vframe_t));
    v->frame_cap = frame_cap;
    v->vaddr = vaddr;
    v->vspace = vspace;
    v->next = NULL;
    if (vframe_table.head == NULL) {
        vframe_table.head = v;
        v->id = 1;
        return v;
    } else {
        // if (!jump_back()) {
        //     v->id = vframe_table.tail->id + 1;
        //     vframe_table.tail->next = v;
        //     vframe_table.tail = v;
        // } else {
            //printf("here\n");
            vframe_t *cur = vframe_table.head;
            while (cur != NULL && cur->next != NULL) {
                if (cur->id + 1 < cur->next->id) {
                    v->id = cur->id + 1;
                    v->next = cur->next;
                    cur->next = v;
                    return v;
                }
                cur = cur->next;
            }
            if (cur != NULL && cur->next == NULL) {
                cur->next = v;
                v->id = cur->id + 1;
                //printf("b:%u\n", v->id);
                return v;
            }
        //}
    }
}

vframe_t *vframe_table_find(vframe_ref_t vframe)
{
    vframe_t *cur = vframe_table.head;
    while (cur != NULL) {
        if (cur->id == vframe) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static void page_out_frame(pframe_t *p)
{
    frame_ref_t fr = p->frame_ref;
    unsigned long pos = p->vframe->id;
    page_out(fr, pos);

    seL4_Error err = cspace_revoke(global_cspace(), frame_page(fr));
    assert(err == 0);
}

static vframe_t *page_in_frame(pframe_t *p, vframe_ref_t vframe)
{
    frame_ref_t fr = p->frame_ref;
    unsigned long pos = vframe;
    page_in(fr, pos);
    
    vframe_t *v = vframe_table_find(vframe);
    assert(v != NULL);
    seL4_Error err = cspace_copy(global_cspace(), v->frame_cap, global_cspace(),
                                frame_page(fr), seL4_AllRights);
    assert(err == 0);
    err = seL4_ARM_Page_Map(v->frame_cap, v->vspace, v->vaddr, seL4_AllRights,
                                seL4_ARM_Default_VMAttributes);
    assert(err == 0);
    p->reference = 1;
    return v;
}

void vframe_pin(frame_ref_t frame_num)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return;
    }
    pframe_t *cur = pframe_table.head;
    while (cur != NULL) {
        if (cur->frame_ref == frame_num) {
            cur->pin = 1;
        }
        cur = cur->next;
    }
}

void vframe_unpin(frame_ref_t frame_num)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul) {
        return;
    }
    pframe_t *cur = pframe_table.head;
    while (cur != NULL) {
        if (cur->frame_ref == frame_num) {
            cur->pin = 1;
        }
        cur = cur->next;
    }
}

void pframe_table_pushback(frame_ref_t frame_ref, vframe_t *vframe)
{
    pframe_t *p = (pframe_t *)malloc(sizeof(pframe_t));
    p->frame_ref = frame_ref;
    p->vframe = vframe;
    p->reference = 1;
    p->pin = 0;
    p->next = NULL;
    if (pframe_table.head == NULL) {
        pframe_table.head = pframe_table.tail = p;
    } else {
        pframe_table.tail->next = p;
        pframe_table.tail = p;
    }
}