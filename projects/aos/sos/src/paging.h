#pragma once

void *pagefile_init(void *p);
int pagefile_ready();
void pagefile_write(char *buf, unsigned long pos);
void pagefile_read(char *buf, unsigned long pos);
