#pragma once

#include <stdint.h>
#include <cspace/cspace.h>

uintptr_t alloc_sos_vaddr(intptr_t user_vaddr, unsigned int size, int *num_pages);
void free_sos_vaddr(uintptr_t sos_vaddr, int num_pages);