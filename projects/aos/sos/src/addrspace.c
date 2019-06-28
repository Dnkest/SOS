#include "addrspace.h"
#include "proc.h"

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

    as->as_page_table = as_page_table_init();
    
    return as;
}

as_region_t *as_create_region(seL4_Word vaddr, size_t memsize, bool read, bool write, bool execute)
{
    as_region_t *region = (as_region_t *)as_alloc_one_page();

    region->vaddr = vaddr;
    region->memsize = memsize;
    region->read = read;
    region->write = write;
    region->execute = execute;

    return region;
}

bool as_check_valid_region(addrspace_t *as, seL4_Word fault_address)
{
    // as_region_t *cur = as->regions;
    // while (cur != NULL) {
    //     if (fault_address >= cur->vaddr && fault_address < cur->vaddr + cur->memsize) {
    //         return true;
    //     }
    //     cur = cur->next;
    // }
    // return false;
    return true;
}

void sos_handle_page_fault(seL4_Word fault_address)
{
    pcb_t *proc = get_cur_proc();

    if (as_check_valid_region(proc->as, fault_address)) {
        cspace_t *cspace = proc->global_cspace;
        seL4_CPtr vspace = proc->vspace;

        frame_ref_t frame = alloc_frame();
        if (frame == NULL_FRAME) {
            ZF_LOGE("Couldn't allocate additional frame");
            //return seL4_NotEnoughMemory;
        }

        seL4_CPtr frame_cptr = cspace_alloc_slot(cspace);
        if (frame_cptr == seL4_CapNull) {
            free_frame(frame);
            ZF_LOGE("Failed to alloc slot for frame");
            //return seL4_NotEnoughMemory;
        }

        seL4_Error err = cspace_copy(cspace, frame_cptr, cspace, frame_page(frame), seL4_AllRights);
        if (err != seL4_NoError) {
            cspace_free_slot(cspace, frame_cptr);
            free_frame(frame);
            ZF_LOGE("Failed to copy cap");
            //return err;
        }
        err = sos_map_frame(cspace, vspace, frame_cptr, frame, proc->as->as_page_table,
                    fault_address & MASK, seL4_AllRights, seL4_ARM_Default_VMAttributes);
        if (err) {
            cspace_delete(cspace, frame_cptr);
            cspace_free_slot(cspace, frame_cptr);
            free_frame(frame);
            ZF_LOGE("Failed to copy cap");
        }

        seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Reply(reply_msg);
    }

}
