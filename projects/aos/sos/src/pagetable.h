#pragma once

#include <sel4/sel4.h>

#include "frame_table.h"
#include "addrspace.h"

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

typedef struct page_table_list {
    seL4_CPtr *cptrs;
    size_t used;
    size_t capacity;
    struct page_table_list *next;
} page_table_list_t;

typedef struct as_page_table {
    page_upper_directory_t **puds;
    page_table_list_t *list;
} as_page_table_t;

as_page_table_t *as_page_table_init();

// seL4_Error sos_alloc_frame(cspace_t *cspace, seL4_CPtr vspace,
//                     as_page_table_t *as_page_table, seL4_Word vaddr,
//                     seL4_CapRights_t rights, seL4_ARM_VMAttributes attr);
        
seL4_Error sos_map_frame(cspace_t *cspace, seL4_CPtr vspace,
                        seL4_CPtr frame_cap, frame_ref_t frame_ref, 
                        as_page_table_t *as_page_table, seL4_Word vaddr, 
                        seL4_CapRights_t rights, seL4_ARM_VMAttributes attr);