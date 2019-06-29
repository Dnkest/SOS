#pragma once

#include <sel4/sel4.h>

#include "frame_table.h"
#include "addrspace.h"

typedef struct accessed_list {
    unsigned int index[BIT(9)];
    unsigned int size;
} accessed_list_t;

typedef struct page_table {
    frame_ref_t *frames;
    seL4_CPtr *frame_caps;
    accessed_list_t *list;
    seL4_CPtr page_table_cptr;
} page_table_t;

typedef struct page_directory {
    page_table_t **pts;
    accessed_list_t *list;
    seL4_CPtr page_directory_cptr;
} page_directory_t;

typedef struct page_upper_directory {
    page_directory_t **pds;
    accessed_list_t *list;
    seL4_CPtr page_upper_directory_cptr;
} page_upper_directory_t;

typedef struct as_page_table {
    page_upper_directory_t **puds;
    accessed_list_t *list;
    //page_table_list_t *list;
} as_page_table_t;

as_page_table_t *as_page_table_init();
seL4_Error as_page_table_destroy(as_page_table_t *table);
        
seL4_Error sos_map_frame(cspace_t *cspace, seL4_CPtr vspace,
                        seL4_CPtr frame_cap, frame_ref_t frame_ref, 
                        as_page_table_t *as_page_table, seL4_Word vaddr, 
                        seL4_CapRights_t rights, seL4_ARM_VMAttributes attr);
