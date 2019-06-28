#pragma once

#include <sel4/sel4.h>
#include <stdbool.h>

#include "pagetable.h"

#define USER_STACK_TOP       (0x800000000000)
#define USER_STACK_PAGES     100
#define USER_IPC_BUFFER      (0x700000000000)
#define USER_HEAP_BASE       (0x710000000000)
#define USER_HEAP_TOP        (0x7fffffffffff)

typedef struct as_region {
    seL4_Word vaddr;
    size_t memsize;
    bool read;
    bool write;
    bool execute;

    struct as_region *next;
} as_region_t;

typedef struct addrspace {
    as_page_table_t *as_page_table;
    as_region_t *regions;
    // as_region_t *heap;
    // as_region_t *stack;
    // as_region_t *ipc_buffer;
} addrspace_t;

uintptr_t as_alloc_one_page();
void as_free(uintptr_t vaddr);

addrspace_t *as_create();

as_region_t *as_create_region(seL4_Word vaddr, size_t memsize, bool read, bool write, bool execute);
void as_define_heap();

void sos_handle_page_fault(seL4_Word fault_address);
