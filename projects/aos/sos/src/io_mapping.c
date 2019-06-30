#include <stdint.h>
#include <adt/linkedlist.h>
#include <utils/page.h>

#include "io_mapping.h"
#include "vmem_layout.h"

#define MASK 0xfffffffff000

static uintptr_t base = SOS_TMP_BUFFER;

static List allocated_list;

uintptr_t alloc_sos_vaddr(intptr_t user_vaddr, unsigned int size, int *num_pages)
{
    uintptr_t base_vaddr = user_vaddr & MASK;
    uintptr_t top_vaddr = (user_vaddr + size) & MASK;
    if (size != (size & MASK)) {
        top_vaddr += PAGE_SIZE_4K;
    }

    *num_pages = (top_vaddr - base_vaddr)/PAGE_SIZE_4K;

    uintptr_t ret = base;

    base += (*num_pages * PAGE_SIZE_4K);

    // uintptr_t ret;
    // if (allocated_list == NULL) {
    //     allocated_list = list_create();
    // }

    // Iterator it = list_iterator(allocated_list);

    // int cur, prev;
    // while (it_has_next(it)) {
    //     cur = (int)it_next(&it);
    //     prev = cur;
    //     if (!it_has_next(it)) {
    //         list_pushback(allocated_list, prev+1);
    //         return SOS_TMP_BUFFER + (prev + 1) * PAGE_SIZE_4K;
    //     }
    //     cur = (int)it_peek(it);
    //     if (cur - prev > num_pages) {
    //         for (int i = 0; i < num_pages; i++) {
    //             list_pushback(allocated_list, prev+1+i);
    //         }
    //         return SOS_TMP_BUFFER + (prev + 1) * PAGE_SIZE_4K;
    //     }
    // }
    // list_pushback(allocated_list, 0);
    return ret;
}

// int int_comparsion(void *a, void *b)
// {
//     return ((int)a == (int)b);
// }

// void free_sos_vaddr(uintptr_t sos_vaddr, int num_pages)
// {
//     int index = (sos_vaddr - SOS_TMP_BUFFER)/PAGE_SIZE_4K;

//     for (int i = index; i < num_pages; i++) {
//         list_delete(allocated_list, index, int_comparsion);
//     }
// }