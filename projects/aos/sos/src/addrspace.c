#include "addrspace.h"
#include "proc.h"
#include "elf.h"

#define MASK 0xFFFFFFFFF000

uintptr_t as_alloc_one_page()
{
    frame_ref_t frame = alloc_frame();
    if (frame == NULL_FRAME) {
        ZF_LOGE("Couldn't allocate additional frame");
        return 0;
    }
    void *ret = (void *)frame_data(frame);
    memset(ret, 0, BIT(seL4_PageBits));
    return (uintptr_t)ret;
}

void as_free(uintptr_t vaddr)
{
    free_frame_address((unsigned char *)vaddr);
}

addrspace_t *as_create()
{
    addrspace_t *as = (addrspace_t *)as_alloc_one_page();

    as->as_page_table = page_table_create();
    as->regions = NULL;
    
    return as;
}

void as_define_region(addrspace_t *as, seL4_Word vaddr, size_t memsize, unsigned long permissions)
{
    as_region_t *region = (as_region_t *)as_alloc_one_page();

    region->base = vaddr & MASK;
    region->top = ((vaddr + memsize) & MASK);
    if (memsize != (memsize & MASK)) {
        region->top += BIT(seL4_PageBits);
    }
    region->read =  permissions & PF_R || permissions & PF_X;
    region->write = permissions & PF_W;
    region->next = NULL;
    printf("Defined region: %p-->%p, %d, %d\n",region->base,region->top,region->read ,region->write);

    if (as->regions == NULL) {
        as->regions = region;
    } else {
        as_region_t *cur = as->regions, *prev;
        while (cur != NULL) {
            prev = cur;

            cur = cur->next;
        }
        prev->next = region;
    }
}

bool as_check_valid_region(addrspace_t *as, seL4_Word fault_address)
{
    if (fault_address == 0) {
        return false;
    }
    as_region_t *cur = as->regions;
    while (cur != NULL) {
        //printf("check region: %p-->%p, %d, %d\n",cur->base,cur->top,cur->read ,cur->write);
        if (fault_address >= cur->base && fault_address < cur->top) {
            return true;
        }
        cur = cur->next;
    }
    return false;
}

bool sos_handle_page_fault(seL4_Word fault_address)
{
    pcb_t *proc = get_cur_proc();
    printf("faultaddress-> %p\n", fault_address);
    if (as_check_valid_region(proc->as, fault_address)) {
        cspace_t *cspace = proc->global_cspace;
        seL4_CPtr vspace = proc->vspace;

        frame_ref_t frame = alloc_frame();
        if (frame == NULL_FRAME) {
            ZF_LOGE("Couldn't allocate additional frame");
            return false;
        }

        seL4_CPtr frame_cptr = cspace_alloc_slot(cspace);
        if (frame_cptr == seL4_CapNull) {
            free_frame(frame);
            ZF_LOGE("Failed to alloc slot for frame");
            return false;
        }

        seL4_Error err = cspace_copy(cspace, frame_cptr, cspace, frame_page(frame), seL4_AllRights);
        if (err != seL4_NoError) {
            cspace_free_slot(cspace, frame_cptr);
            free_frame(frame);
            ZF_LOGE("Failed to copy cap");
            return false;
        }
        err = sos_map_frame(cspace, vspace, frame_cptr, frame, proc->as->as_page_table,
                    fault_address & MASK, seL4_AllRights, seL4_ARM_Default_VMAttributes);
        if (err) {
            printf("error = %d\n", err);
            cspace_delete(cspace, frame_cptr);
            cspace_free_slot(cspace, frame_cptr);
            free_frame(frame);
            ZF_LOGE("Failed to copy cap");
            return false;
        }
        seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Reply(reply_msg);
        return true;
    }
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_Reply(reply_msg);
    return false;
}
