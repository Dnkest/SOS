#include <stdio.h>
#include <utils/page.h>
#include <picoro/picoro.h>

#include "uio.h"
#include "utils/kmalloc.h"
#include "utils/circular_id.h"
#include "vmem_layout.h"
#include "globals.h"
#include "vframe_table.h"

static circular_id_t *mapping_addr_table;

static char lock = 0;

struct uio {
    proc_t *proc;

    seL4_Word vaddr;
    unsigned int num_pages;
    seL4_CPtr *frame_caps;
    frame_ref_t *frame_refs;
};

uio_t *uio_init(proc_t *proc)
{
    if (mapping_addr_table == NULL) { mapping_addr_table = circular_id_init(SOS_UIO, PAGE_SIZE_4K, 10); }
    uio_t *ret = (uio_t *)kmalloc(sizeof(uio_t));
    ret->proc = proc;
    return ret;
}

seL4_Word uio_map(uio_t *uio, seL4_Word user_vaddr, seL4_Word size)
{
    while (lock == 1) { yield(0); }
    lock = 1;

    proc_t *proc = uio->proc;

    seL4_Word user_base_vaddr = user_vaddr & 0xFFFFFFFFF000;
    unsigned int num_pages = (user_vaddr - user_base_vaddr + size) / PAGE_SIZE_4K;
    if ((user_vaddr - user_base_vaddr + size) % PAGE_SIZE_4K) {
        num_pages++;
    }
    uio->num_pages = num_pages;
    uio->frame_caps = (seL4_CPtr *)kmalloc(num_pages *sizeof(seL4_CPtr));
    uio->frame_refs = (frame_ref_t *)kmalloc(num_pages *sizeof(frame_ref_t));

    seL4_Word kernel_base_vaddr = circular_id_alloc(mapping_addr_table, num_pages);
    uio->vaddr = kernel_base_vaddr;

    seL4_Word kernel_vaddr_tmp = kernel_base_vaddr, user_vaddr_tmp = user_base_vaddr;
    for (unsigned int i = 0; i < num_pages; i++) {

        //printf("mapping %p --> %p (%u/%u)\n", user_vaddr_tmp, kernel_vaddr_tmp, i+1, num_pages);
        seL4_CPtr kernel_frame_cap = cspace_alloc_slot(global_cspace());

        if (kernel_frame_cap == seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for stack");
            return 0;
        }
        vframe_ref_t vframe = addrspace_lookup_vframe(process_addrspace(proc), user_vaddr_tmp);
        if (vframe == NULL_FRAME) {
            seL4_CPtr frame_cap = cspace_alloc_slot(global_cspace());
            if (frame_cap == seL4_CapNull) {
                free_frame(frame_cap);
                ZF_LOGE("Failed to alloc slot for frame");
            }

            seL4_Error err = addrspace_alloc_map_one_page(process_addrspace(proc), global_cspace(),
                                frame_cap, process_vspace(proc), user_vaddr_tmp);
            if (err) {
                ZF_LOGE("Failed to copy cap");
            }
            vframe = addrspace_lookup_vframe(process_addrspace(proc), user_vaddr_tmp);
        }
        frame_ref_t fr = frame_from_vframe(vframe);
        
        vframe_pin(fr);    
        uio->frame_refs[i] = fr;
        seL4_Error err = cspace_copy(global_cspace(), kernel_frame_cap, global_cspace(),
                            frame_page(fr), seL4_AllRights);
        assert(err == 0);
        err = map_frame(global_cspace(), kernel_frame_cap, seL4_CapInitThreadVSpace,
                        kernel_vaddr_tmp, seL4_AllRights, seL4_ARM_Default_VMAttributes);
        assert(err == 0);

        if (err) {
            ZF_LOGE("mapping user address failed");
            return 0;
        }
        uio->frame_caps[i] = kernel_frame_cap;

        user_vaddr_tmp += PAGE_SIZE_4K;
        kernel_vaddr_tmp += PAGE_SIZE_4K;
    }
    seL4_Word ret = kernel_base_vaddr + user_vaddr - user_base_vaddr;
    lock = 0;
    return ret;
}

void uio_unmap(uio_t *uio)
{
    while (lock == 1) { yield(0); }
    lock = 1;

    for (unsigned int i = 0; i < uio->num_pages; i++) {

        seL4_CPtr frame_cap = uio->frame_caps[i];
        seL4_Error err = seL4_ARM_Page_Unmap(frame_cap);
        assert(err == seL4_NoError);

        /* delete the copy of the stack frame cap */
        err = cspace_delete(global_cspace(), frame_cap);
        assert(err == seL4_NoError);

        /* mark the slot as free */
        cspace_free_slot(global_cspace(), frame_cap);

        vframe_unpin(uio->frame_refs[i]);
    }

    kfree(uio->frame_caps);
    kfree(uio->frame_refs);
    circular_id_free(mapping_addr_table, uio->vaddr, uio->num_pages);
    lock = 0;
}

void uio_destroy(uio_t *uio)
{
    uio_unmap(uio);
    kfree(uio);
}
