#include "proc.h"

/* Used for debuggin. */
void print_error(seL4_Error err);

accessed_list_t *accessed_list_init()
{
    accessed_list_t *list = (accessed_list_t *)as_alloc_one_page();
    list->size = 0;

    return list;
}

void accessed_list_append(accessed_list_t *list, int index)
{
    list->index[list->size++] = index;
}

as_page_table_t *page_table_create()
{
    as_page_table_t *page_table = (as_page_table_t *)as_alloc_one_page();
    page_table->puds = (page_upper_directory_t **)as_alloc_one_page();
    page_table->list = accessed_list_init();
    return page_table;
}

page_upper_directory_t *pud_init()
{
    page_upper_directory_t *p_pud = (page_upper_directory_t *)as_alloc_one_page();
    p_pud->pds = (page_directory_t **)as_alloc_one_page();
    p_pud->list = accessed_list_init();

    return p_pud;
}

page_directory_t *pd_init()
{
    page_directory_t *p_pd = (page_directory_t *)as_alloc_one_page();
    p_pd->pts = (page_table_t **)as_alloc_one_page();
    p_pd->list = accessed_list_init();

    return p_pd;
}

page_table_t *pt_init()
{
    page_table_t *p_pt = (page_table_t *)as_alloc_one_page();
    p_pt->frames = (frame_ref_t *)as_alloc_one_page();
    p_pt->frame_caps = (seL4_CPtr *)as_alloc_one_page();
    p_pt->list = accessed_list_init();

    return p_pt;
}

/* Helper function to allocate 4kiB of specfied type. */
seL4_CPtr alloc_type(cspace_t *cspace, seL4_Word type, ut_t *ut)
{
    ut = ut_alloc_4k_untyped(NULL);
    if (ut == NULL) {
        ZF_LOGE("Out of 4k untyped");
        return seL4_CapNull;
    }

    /* allocate a slot to retype the memory for object into */
    seL4_CPtr cptr = cspace_alloc_slot(cspace);
    if (cptr == seL4_CapNull) {
        ut_free(ut);
        ZF_LOGE("Failed to allocate slot");
        return seL4_CapNull;
    }

    /* now do the retype */
    seL4_Error err = cspace_untyped_retype(cspace, ut->cap, cptr, type, seL4_PageBits);
    if (err != seL4_NoError) {
        ut_free(ut);
        cspace_free_slot(cspace, cptr);
        ZF_LOGE("Failed retype untyped");
        return 0;
    }
    return cptr;
}

seL4_Error map_frame_new(cspace_t *cspace, seL4_CPtr vspace,
                        seL4_CPtr frame_cap, frame_ref_t frame_ref,
                        page_table_t *pt, seL4_Word vaddr,
                        seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 12) & 0b111111111;
    if (!pt->frames[index]) {
        seL4_Error err = seL4_ARM_Page_Map(frame_cap, vspace, vaddr, rights, attr);
        if (err == seL4_DeleteFirst) {
            printf("pagetable error: %p is already mapped (this should only happen once!)\n", vaddr);
        } else if (err) {
            return err;
        }
        pt->frames[index] = frame_ref;
        pt->frame_caps[index] = frame_cap;
        accessed_list_append(pt->list, index);
    }
    return seL4_NoError;
}

seL4_Error map_pt(cspace_t *cspace, seL4_CPtr vspace,
                seL4_CPtr frame_cap, frame_ref_t frame_ref,
                page_directory_t *pd, seL4_Word vaddr,
                seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 21) & 0b111111111;
    if (!pd->pts[index]) {
        page_table_t *p_pt = pt_init();

        ut_t *ut;
        seL4_CPtr cptr = alloc_type(cspace, seL4_ARM_PageTableObject, ut);
        if (cptr == 0) {
            ZF_LOGE("Failed to alloc 4k of type seL4_ARM_PageTableObject");
            return seL4_NotEnoughMemory;
        }
        
        seL4_Error err = seL4_ARM_PageTable_Map(cptr, vspace, vaddr, attr);
        if (err == seL4_DeleteFirst) {
            printf("pagetable error: %p is already mapped (this should only happen once!)\n", vaddr);
        } else if (err) {
            return err;
        }
        p_pt->page_table_cptr = cptr;
        p_pt->page_table_ut = ut;
        pd->pts[index] = p_pt;
        accessed_list_append(pd->list, index);

        return map_frame_new(cspace, vspace, frame_cap, frame_ref, p_pt, vaddr, rights, attr);
    } else {
        return map_frame_new(cspace, vspace, frame_cap, frame_ref, pd->pts[index], vaddr, rights, attr);
    }
    return seL4_NoError;
}

seL4_Error map_pd(cspace_t *cspace, seL4_CPtr vspace,
                seL4_CPtr frame_cap, frame_ref_t frame_ref,
                page_upper_directory_t *pud, seL4_Word vaddr,
                seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 30) & 0b111111111;
    if (!pud->pds[index]) {
        page_directory_t *p_pd = pd_init();

        ut_t *ut;
        seL4_CPtr cptr = alloc_type(cspace, seL4_ARM_PageDirectoryObject, ut);
        if (cptr == 0) {
            ZF_LOGE("Failed to alloc 4k of type seL4_ARM_PageDirectoryObject");
            return seL4_NotEnoughMemory;
        }
        
        seL4_Error err = seL4_ARM_PageDirectory_Map(cptr, vspace, vaddr, attr);
        if (err == seL4_DeleteFirst) {
            printf("pagetable error: %p is already mapped (this should only happen once!)\n", vaddr);
        } else if (err) {
            return err;
        }
        p_pd->page_directory_cptr = cptr;
        p_pd->page_directory_ut = ut;
        pud->pds[index] = p_pd;
        accessed_list_append(pud->list, index);

        return map_pt(cspace, vspace, frame_cap, frame_ref, p_pd, vaddr, rights, attr);
    } else {
        return map_pt(cspace, vspace, frame_cap, frame_ref, pud->pds[index], vaddr, rights, attr);
    }
    return seL4_NoError;
}

seL4_Error sos_map_frame(cspace_t *cspace, seL4_CPtr vspace,
                        seL4_CPtr frame_cap, frame_ref_t frame_ref,
                        as_page_table_t *as_page_table, seL4_Word vaddr,
                        seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 39) & 0b111111111;
    // printf("vaddr = %p, index = %d\n", vaddr, index);
    if (!as_page_table->puds[index]) {
        page_upper_directory_t *p_pud = pud_init();

        ut_t *ut;
        seL4_CPtr cptr = alloc_type(cspace, seL4_ARM_PageUpperDirectoryObject, ut);
        if (cptr == 0) {
            printf("pagetable: 2\n");
            ZF_LOGE("Failed to alloc 4k of type seL4_ARM_PageUpperDirectoryObject");
            return seL4_NotEnoughMemory;
        }

        seL4_Error err = seL4_ARM_PageUpperDirectory_Map(cptr, vspace, vaddr, attr);
        if (err == seL4_DeleteFirst) {
            printf("pagetable error: %p is already mapped (this should only happen once!)\n", vaddr);
        } else if (err) {
            return err;
        }
        p_pud->page_upper_directory_cptr = cptr;
        p_pud->page_upper_directory_ut = ut;
        as_page_table->puds[index] = p_pud;
        accessed_list_append(as_page_table->list, index);

        return map_pd(cspace, vspace, frame_cap, frame_ref, p_pud, vaddr, rights, attr);
    } else {
        return map_pd(cspace, vspace, frame_cap, frame_ref, as_page_table->puds[index], vaddr, rights, attr);
    }
    return seL4_NoError;
}

seL4_CPtr lookup_frame(as_page_table_t *as_page_table, seL4_Word vaddr)
{
    //printf("lookup_frame: 1\n");
    int index = (vaddr >> 39) & 0b111111111;
    page_upper_directory_t *p_pud = as_page_table->puds[index];
    if (p_pud) {
        //printf("lookup_frame: 2\n");
        index = (vaddr >> 30) & 0b111111111;
        page_directory_t *p_pd = p_pud->pds[index];
        if (p_pd) {
            //printf("lookup_frame: 3\n");
            index = (vaddr >> 21) & 0b111111111;
            page_table_t *p_pt = p_pd->pts[index];
            if (p_pt) {
                //printf("lookup_frame: 4\n");
                index = (vaddr >> 12) & 0b111111111;
                return p_pt->frame_caps[index];
            }
        }
    }
    return seL4_CapNull;
}

seL4_Error page_table_destroy(cspace_t *cspace, as_page_table_t *table)
{
    seL4_Error err;
    for (unsigned int i = table->list->index[0]; i < table->list->size; i++) {
        page_upper_directory_t *p_pud = table->puds[i];
        for (unsigned int i = p_pud->list->index[0]; i < p_pud->list->size; i++) {
            page_directory_t *p_pd = p_pud->pds[i];
            for (unsigned int i = p_pd->list->index[0]; i < p_pd->list->size; i++) {
                page_table_t *p_pt = p_pd->pts[i];
                for (unsigned int i = p_pt->list->index[0]; i < p_pt->list->size; i++) {
                    err = seL4_ARM_Page_Unmap(p_pt->frame_caps[i]);
                    if (err) {
                        return err;
                    }
                    free_frame(p_pt->frames[i]);
                    cspace_delete(cspace, p_pt->frame_caps[i]);
                    cspace_free_slot(cspace, p_pt->frame_caps[i]);
                }
                as_free(p_pt->list);
                as_free(p_pt->frames);
                as_free(p_pt->frame_caps);
                err = seL4_ARM_PageTable_Unmap(p_pt->page_table_cptr);
                if (err) {
                    return err;
                }
                cspace_delete(cspace, p_pt->page_table_cptr);
                cspace_free_slot(cspace, p_pt->page_table_cptr);
                ut_free(p_pt->page_table_ut);
                as_free(p_pt);
            }
            as_free(p_pd->pts);
            as_free(p_pd->list);
            err = seL4_ARM_PageDirectory_Unmap(p_pd->page_directory_cptr);
            if (err) {
                return err;
            }
            cspace_delete(cspace, p_pd->page_directory_cptr);
            cspace_free_slot(cspace, p_pd->page_directory_cptr);
            ut_free(p_pd->page_directory_ut);
            as_free(p_pd);
        }
        as_free(p_pud->pds);
        as_free(p_pud->list);
        err = seL4_ARM_PageUpperDirectory_Unmap(p_pud->page_upper_directory_cptr);
        if (err) {
            return err;
        }
        cspace_delete(cspace, p_pud->page_upper_directory_cptr);
        cspace_free_slot(cspace, p_pud->page_upper_directory_cptr);
        ut_free(p_pud->page_upper_directory_ut);
        as_free(p_pud);
    }
    as_free(table->puds);
    as_free(table->list);
    return seL4_NoError;
}
