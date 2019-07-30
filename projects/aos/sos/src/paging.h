#pragma once

#include "frame_table.h"

void *paging_init(void *p);
int paging_ready();

void page_out(frame_ref_t frame, unsigned long pos);
void page_in(frame_ref_t frame, unsigned long pos);
