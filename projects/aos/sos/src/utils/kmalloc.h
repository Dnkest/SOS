#pragma once

#include <stdlib.h>

void *kmalloc(size_t size);
void kfree(void *p);
void kmalloc_tests();
