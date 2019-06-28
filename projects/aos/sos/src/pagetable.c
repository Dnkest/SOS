#include "proc.h"

/* Used for debuggin. */
void print_error(seL4_Error err);

page_table_list_t *pt_list_init()
{
    page_table_list_t *list = (page_table_list_t *)as_alloc_one_page();
    seL4_CPtr *cptrs = (seL4_CPtr *)as_alloc_one_page();
    list->cptrs = cptrs;
    list->used = 0;
    list->capacity = BIT(12)/sizeof(seL4_CPtr);
    list->next = NULL;

    return list;
}

void pt_list_push_back(page_table_list_t *list, seL4_CPtr cptr)
{
    while (list->used == list->capacity) {
        /* Find a list that's not full. */
        list = list->next;
    }

    list->cptrs[list->used++] = cptr;
    if (list->used == list->capacity) {
        /* If list becomes full after pushback, allocate a new one and points to it. */
        page_table_list_t *next = pt_list_init();
        list->next = next;
    }
}

as_page_table_t *as_page_table_init()
{
    as_page_table_t *page_table = (as_page_table_t *)as_alloc_one_page();
    page_table->puds = (page_upper_directory_t **)as_alloc_one_page();
    page_table->list = pt_list_init();
    return page_table;
}

/* Helper function to allocate 4kiB of specfied type. */
seL4_CPtr alloc_type(page_table_list_t *list, cspace_t *cspace, seL4_Word type)
{
    /* Assume the error was because we are missing a paging structure */
    ut_t *ut = ut_alloc_4k_untyped(NULL);
    if (ut == NULL) {
        ZF_LOGE("Out of 4k untyped");
        seL4_DebugPutChar('a');
        return 0;
    }

    /* allocate a slot to retype the memory for object into */
    seL4_CPtr cptr = cspace_alloc_slot(cspace);
    if (cptr == seL4_CapNull) {
        ut_free(ut);
        seL4_DebugPutChar('b');
        ZF_LOGE("Failed to allocate slot");
        return 0;
    }

    /* now do the retype */
    seL4_Error err = cspace_untyped_retype(cspace, ut->cap, cptr, type, seL4_PageBits);
    if (err != seL4_NoError) {
        ut_free(ut);
        cspace_free_slot(cspace, cptr);
        ZF_LOGE("Failed retype untyped");
        seL4_DebugPutChar('c');
        return 0;
    }
    pt_list_push_back(list, cptr);

    return cptr;
}

seL4_Error map_frame_new(cspace_t *cspace, seL4_CPtr vspace,
                        seL4_CPtr frame_cap, frame_ref_t frame_ref,
                        page_table_list_t *list, page_table_t *pt, seL4_Word vaddr,
                        seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 12) & 0b111111111;
    if (!pt->frames[index]) {
        seL4_DebugPutChar('1');
        seL4_Error err = seL4_ARM_Page_Map(frame_cap, vspace, vaddr, rights, attr);
        if (err == seL4_DeleteFirst) {
            seL4_DebugPutChar('Q');
        } else if (err) {
            print_error(err);
            return err;
        }
        pt->frames[index] = frame_ref;
        pt_list_push_back(list, frame_cap);
    }
    return seL4_NoError;
}

seL4_Error map_pt(cspace_t *cspace, seL4_CPtr vspace,
                seL4_CPtr frame_cap, frame_ref_t frame_ref,
                page_table_list_t *list, page_directory_t *pd, seL4_Word vaddr,
                seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 21) & 0b111111111;
    if (!pd->pts[index]) {
        seL4_DebugPutChar('2');
        page_table_t *p_pt = (page_table_t *)as_alloc_one_page();
        p_pt->frames = (frame_ref_t *)as_alloc_one_page();

        seL4_CPtr cptr = alloc_type(list, cspace, seL4_ARM_PageTableObject);
        if (cptr == 0) {
            ZF_LOGE("Failed to alloc 4k of type seL4_ARM_PageTableObject");
        }
        
        seL4_Error err = seL4_ARM_PageTable_Map(cptr, vspace, vaddr, attr);
        if (err == seL4_DeleteFirst) {
            seL4_DebugPutChar('Q');
        } else if (err) {
            print_error(err);
            return err;
        }
        p_pt->page_table_cptr = cptr;

        pd->pts[index] = p_pt;
        return map_frame_new(cspace, vspace, frame_cap, frame_ref,
                            list, p_pt, vaddr, rights, attr);
    } else {
        return map_frame_new(cspace, vspace, frame_cap, frame_ref,
                            list, pd->pts[index], vaddr, rights, attr);
    }
    return seL4_NoError;
}

seL4_Error map_pd(cspace_t *cspace, seL4_CPtr vspace,
                seL4_CPtr frame_cap, frame_ref_t frame_ref,
                page_table_list_t *list, page_upper_directory_t *pud, seL4_Word vaddr,
                seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 30) & 0b111111111;
    if (!pud->pds[index]) {
        seL4_DebugPutChar('3');
        page_directory_t *p_pd = (page_directory_t *)as_alloc_one_page();
        p_pd->pts = (page_table_t **)as_alloc_one_page();

        seL4_CPtr cptr = alloc_type(list, cspace, seL4_ARM_PageDirectoryObject);
        if (cptr == 0) {
            ZF_LOGE("Failed to alloc 4k of type seL4_ARM_PageDirectoryObject");
        }
        
        seL4_Error err = seL4_ARM_PageDirectory_Map(cptr, vspace, vaddr, attr);
        if (err == seL4_DeleteFirst) {
            seL4_DebugPutChar('Q');
        } else if (err) {
            print_error(err);
            return err;
        }
        p_pd->page_directory_cptr = cptr;
        
        pud->pds[index] = p_pd;
        return map_pt(cspace, vspace, frame_cap, frame_ref,
                    list, p_pd, vaddr, rights, attr);
    } else {
        return map_pt(cspace, vspace, frame_cap, frame_ref,
                    list, pud->pds[index], vaddr, rights, attr);
    }
    return seL4_NoError;
}

void print_error(seL4_Error err)
{
    switch (err) {
    case seL4_InvalidArgument:
        seL4_DebugPutChar('a');
        break;
    case seL4_InvalidCapability:
        seL4_DebugPutChar('b');
        break;
    case seL4_IllegalOperation:
        seL4_DebugPutChar('c');
        break;
    case seL4_RangeError:
        seL4_DebugPutChar('d');
        break;
    case seL4_AlignmentError:
        seL4_DebugPutChar('e');
        break;
    case seL4_FailedLookup:
        seL4_DebugPutChar('f');
        break;
    case seL4_TruncatedMessage:
        seL4_DebugPutChar('g');
        break;
    case seL4_DeleteFirst:
        seL4_DebugPutChar('h');
        break;
    case seL4_RevokeFirst:
        seL4_DebugPutChar('i');
        break;
    case seL4_NotEnoughMemory:
        seL4_DebugPutChar('j');
        break;
    }
}

seL4_Error sos_map_frame(cspace_t *cspace, seL4_CPtr vspace,
                        seL4_CPtr frame_cap, frame_ref_t frame_ref,
                        as_page_table_t *as_page_table, seL4_Word vaddr,
                        seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 39) & 0b111111111;
    if (!as_page_table->puds[index]) {
        page_upper_directory_t *p_pud = (page_upper_directory_t *)as_alloc_one_page();
        p_pud->pds = (page_directory_t **)as_alloc_one_page();
        seL4_CPtr cptr = alloc_type(as_page_table->list, cspace, seL4_ARM_PageUpperDirectoryObject);
        if (cptr == 0) {
            ZF_LOGE("Failed to alloc 4k of type seL4_ARM_PageUpperDirectoryObject");
        }
        seL4_Error err = seL4_ARM_PageUpperDirectory_Map(cptr, vspace, vaddr, attr);
        if (err == seL4_DeleteFirst) {
            seL4_DebugPutChar('Q');
        } else if (err) {
            print_error(err);
            return err;
        }
        p_pud->page_upper_directory_cptr = cptr;
        as_page_table->puds[index] = p_pud;
        return map_pd(cspace, vspace, frame_cap, frame_ref,
                    as_page_table->list, p_pud, vaddr, rights, attr);
    } else {
        return map_pd(cspace, vspace, frame_cap, frame_ref,
                    as_page_table->list, as_page_table->puds[index], vaddr, rights, attr);
    }
    return seL4_NoError;
}

