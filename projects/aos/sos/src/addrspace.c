#include "addrspace.h"
#include "frame_table.h"

typedef struct page_table {
    frame_ref_t *frames;
    seL4_CPtr page_table_cptr;
} page_table_t;

typedef struct page_directory {
    page_table_t **pts;
    seL4_CPtr page_directory_cptr;
} page_directory_t;

typedef struct page_upper_directory {
    page_directory_t **pds;
    seL4_CPtr page_upper_directory_cptr;
} page_upper_directory_t;

typedef struct addrspace {
    page_upper_directory_t **puds;
    cspace_t *cspace;
    seL4_CPtr vspace;
} addrspace_t;

uintptr_t as_alloc_one_page()
{
    uintptr_t ret = (uintptr_t)frame_data(alloc_frame());
    memset(ret, 0, BIT(seL4_PageBits));
    return ret;
}

/* Helper function to allocate 4kiB of specfied type. */
seL4_CPtr alloc_type(cspace_t *cspace, seL4_Word type);

addrspace_t *as_create(cspace_t *cspace, seL4_CPtr vspace)
{
    addrspace_t *as = as_alloc_one_page();
    if (as == NULL) {
        return NULL;
	}

    as->puds = as_alloc_one_page();
    as->cspace = cspace;
    as->vspace = vspace;

    return as;
}

seL4_Error map_pt(cspace_t *cspace, seL4_CPtr vspace, page_table_t *pt, seL4_Word vaddr)
{
    seL4_CPtr cptr = alloc_type(cspace, seL4_ARM_PageTableObject);
    if (cptr == NULL) {
        ZF_LOGE("Failed to alloc 4k of specific type");
    }

    seL4_Error error = seL4_ARM_PageTable_Map(cptr, vspace, vaddr, seL4_ARM_Default_VMAttributes);
    pt->page_table_cptr = cptr;
    return error;
}

seL4_Error map_pd(cspace_t *cspace, seL4_CPtr vspace, page_directory_t *pd, seL4_Word vaddr)
{
    int index = (vaddr >> 12) & 0b111111111;
    if (!pd->pts[index]) {
        /* allocate next level page table if not exists. */
        page_table_t *p_pt = as_alloc_one_page();
        pd->pts[index] = p_pt;

        seL4_Error error = map_pt(cspace, vspace, p_pt, vaddr);
        if (error) {
            return error;
        }

        seL4_CPtr cptr = alloc_type(cspace, seL4_ARM_PageDirectoryObject);
        if (cptr == NULL) {
            ZF_LOGE("Failed to alloc 4k of specific type");
        }

        error = seL4_ARM_PageDirectory_Map(cptr, vspace, vaddr, seL4_ARM_Default_VMAttributes);
        pd->page_directory_cptr = cptr;
        return error;
    }
    /* already mapped. */
    return 0;
}

seL4_Error map_pud(cspace_t *cspace, seL4_CPtr vspace, page_upper_directory_t *pud, seL4_Word vaddr)
{
    int index = (vaddr >> 21) & 0b111111111;
    if (!pud->pds[index]) {
        /* allocate next level page table if not exists. */
        page_directory_t *p_pd = as_alloc_one_page();
        pud->pds[index] = p_pd;

        seL4_Error error = map_pd(cspace, vspace, p_pd, vaddr);
        if (error) {
            return error;
        }

        seL4_CPtr cptr = alloc_type(cspace, seL4_ARM_PageUpperDirectoryObject);
        if (cptr == NULL) {
            ZF_LOGE("Failed to alloc 4k of specific type");
        }

        error = seL4_ARM_PageUpperDirectory_Map(cptr, vspace, vaddr, seL4_ARM_Default_VMAttributes);
        pud->page_upper_directory_cptr = cptr;
        return error;
    }
    /* already mapped. */
    return 0;
}

seL4_Error sos_map_frame(addrspace_t *as, cspace_t *cspace, seL4_CPtr frame_cap, seL4_CPtr vspace,
                            seL4_Word vaddr, seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    int index = (vaddr >> 30) & 0b111111111;
    if (!as->puds[index]) {
        /* allocate next level page table if not exists. */
        page_upper_directory_t *p_pud = as_alloc_one_page();
        as->puds[index] = as_alloc_one_page();

        seL4_Error error = map_pud(as->cspace, as->vspace, p_pud, vaddr);
        if (error) {
            return error;
        }

        return seL4_ARM_Page_Map(frame_cap, vspace, vaddr, rights, attr);
    }
    /* already mapped. */
    return 0;
}

seL4_CPtr alloc_type(cspace_t *cspace, seL4_Word type)
{
    /* Assume the error was because we are missing a paging structure */
    ut_t *ut = ut_alloc_4k_untyped(NULL);
    if (ut == NULL) {
        ZF_LOGE("Out of 4k untyped");
        return NULL;
    }

    /* allocate a slot to retype the memory for object into */
    seL4_CPtr cptr = cspace_alloc_slot(cspace);
    if (cptr == seL4_CapNull) {
        ut_free(ut);
        ZF_LOGE("Failed to allocate slot");
        return NULL;
    }

    /* now do the retype */
    seL4_Error err = cspace_untyped_retype(cspace, ut->cap, cptr, type, seL4_PageBits);
    ZF_LOGE_IFERR(err, "Failed retype untyped");
    if (err != seL4_NoError) {
        ut_free(ut);
        cspace_free_slot(cspace, cptr);
        return NULL;
    }
    return cptr;
}
