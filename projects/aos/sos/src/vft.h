#pragma once

#include "frame_table.h"

typedef unsigned long vframe_ref_t;
//frame_ref_t virtual_alloc_frame(frame_ref_t f);
void print_limit();
frame_ref_t frame_ref_from_v(vframe_ref_t vframe);
vframe_ref_t valloc_frame();
void vframe_add_cap(vframe_ref_t vframe, seL4_Word frame_cap);
int vft_set_reference(vframe_ref_t vframe, seL4_CPtr vspace, seL4_Word vaddr);
