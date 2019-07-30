#pragma once

void *paging_init(void *p);
int paging_ready();

void page_out(const char *buf, unsigned long pos);
void page_in(char *buf, unsigned long pos);
