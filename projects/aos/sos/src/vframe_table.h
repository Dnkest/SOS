#pragma once

#include "frame_table.h"

typedef unsigned long vframe_ref_t;

vframe_ref_t alloc_vframe(seL4_CPtr frame_cap, seL4_Word vaddr, seL4_CPtr vspace);
frame_ref_t frame_from_vframe(vframe_ref_t vframe);
void free_vframe(vframe_ref_t vframe);

void vframe_pin(frame_ref_t frame_num);
void vframe_unpin(frame_ref_t frame_num);
