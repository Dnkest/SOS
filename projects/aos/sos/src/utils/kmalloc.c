#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "kmalloc.h"
#include "../mapping.h"
#include "../ut.h"
#include "../globals.h"
#include "../vmem_layout.h"

#define CAPACITY ((1<<12) - sizeof(struct _list *) - sizeof(int))/sizeof(node)

static uintptr_t base = SOS_KMALLOC;
static uintptr_t data_base = SOS_KMALLOC_DATA;

static void *bump(size_t size, int flag)
{
    //assert(size % 4096 == 0);
    // printf("size %u(%d)\n", size, flag);
    int num_pages = size/4096;

    uintptr_t ret;
    if (flag) {
        ret = base;
        base += size;
    } else {
        ret = data_base;
        data_base += size;
    }

    for (int i = 0; i < num_pages; i++) {

        /* Allocate an untyped for the frame. */
        ut_t *ut = ut_alloc_4k_untyped(NULL);
        if (ut == NULL) {
            return seL4_CapNull;
        }

        /* Allocate a slot for the page capability. */
        seL4_ARM_Page cptr = cspace_alloc_slot(global_cspace());
        if (cptr == seL4_CapNull) {
            ut_free(ut);
            return seL4_CapNull;
        }

        /* Retype the untyped into a page. */
        seL4_Word err = cspace_untyped_retype(global_cspace(), ut->cap, cptr, seL4_ARM_SmallPageObject, seL4_PageBits);
        if (err != 0) {
            cspace_free_slot(global_cspace(), cptr);
            ut_free(ut);
            return seL4_CapNull;
        }
        /* Map the frame into SOS. */
        seL4_ARM_VMAttributes attrs = seL4_ARM_Default_VMAttributes | seL4_ARM_ExecuteNever;
        err = map_frame(global_cspace(), cptr, global_vspace(), ret + i * 4096, seL4_ReadWrite, attrs);
        if (err != 0) {
            cspace_delete(global_cspace(), cptr);
            cspace_free_slot(global_cspace(), cptr);
            ut_free(ut);
            return seL4_CapNull;
        }
    }

    return (void *)ret;
}

typedef struct _node {
    void *location;
    size_t size;
} node, *p_node;

typedef struct _list {
    node nodes[CAPACITY];
    int size;
    struct _list *next;
} list;

static list *allocated_list = NULL;
static list *free_list = NULL;

list *list_ini()
{
    list *new_list = (list *)bump(4096, 1);
    new_list->size = 0;
    new_list->next = NULL;
    return new_list;
}

void list_appen(list *l, void *p, size_t size)
{
    while (l->size == CAPACITY) {
        l = l->next;
    }
    node n = { .location = p,
                .size = size };
    l->nodes[l->size++] = n;
    if (l->size == CAPACITY) {
        l->next = list_ini();
    }
}

void list_del_index(list *c, int i)
{
    if (c->size == 1) {
        --c->size;
        return;
    }
    list *t = c;
    while (t->next != NULL && t->next->size != 0) {
        t = t->next;
    }
    c->nodes[i] = t->nodes[--t->size];
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size = (size % 32) ? (size / 32 + 1) * 32 : size;

    if (free_list == NULL) {
        free_list = list_ini();
    }

    if (allocated_list == NULL) {
        allocated_list = list_ini();
    }

    list *c = free_list;
    void *ret;
    while (c != NULL) {
        for (int i = c->size-1; i >= 0; i--) {
            if (c->nodes[i].size > size) {
                ret = c->nodes[i].location;
                memset(ret, 0, size);
                c->nodes[i].location += size;
                c->nodes[i].size -= size;
                list_appen(allocated_list, ret, size);
                return ret;
            } else if (c->nodes[i].size == size) {
                ret = c->nodes[i].location;
                memset(ret, 0, size);
                list_del_index(c, i);
                list_appen(allocated_list, ret, size);
                return ret;
            }
        }
        c = c->next;
    }

    size_t bump_size = (size % (1 << 12)) ? (size / (1 << 12) + 1) * (1 << 12) : size;

    void *new = bump(bump_size, 0);
    list_appen(allocated_list, new, size);
    if (bump_size > size) {
        list_appen(free_list, new + size, bump_size - size);
    }
    return new;
}

void debug_print_l(list *c)
{
    while (c != NULL) {
        for (int i = 0; i < c->size; i++) {
            printf("%d:[%p, %lu, %p]\n", i, c->nodes[i].location, c->nodes[i].size, c->nodes[i].size);
        }
        c = c->next;
    }
}

void debug_print()
{
    printf("\n\n=============================allocated==================================\n");
    debug_print_l(allocated_list);
    printf("==============================free======================================\n");
    debug_print_l(free_list);
    printf("========================================================================\n\n");
}

void kfree(void *p)
{
    //printf("freeing %p\n", p);
    if (allocated_list == NULL || free_list == NULL) {
        return;
    }

    size_t s = 0;
    list *c = allocated_list;
    while (c != NULL) {
        for (int i = 0; i < c->size; i++) {
            if (c->nodes[i].location == p) {
                s = c->nodes[i].size;
                list_del_index(c, i);
            }
        }
        c = c->next;
    }
    if (s == 0) { return; }

    int b = 0;
    c = free_list;
    while (c != NULL) {
        for (int i = 0; i < c->size; i++) {
            if (c->nodes[i].location + c->nodes[i].size == p) {
                c->nodes[i].size += s;
                s = c->nodes[i].size;
                p = c->nodes[i].location;
                b = 1;
            }
        }
        c = c->next;
    }

    int a = 0;
    c = free_list;
    while (c != NULL) {
        for (int i = 0; i < c->size; i++) {
            if (c->nodes[i].location == p + s) {
                c->nodes[i].location = p;
                c->nodes[i].size += s;
                a = 1;
            }
        }
        c = c->next;
    }

    if (a != 0) {
        c = free_list;
        while (c != NULL) {
            for (int i = 0; i < c->size; i++) {
                if (c->nodes[i].location == p && c->nodes[i].size == s) {
                    list_del_index(c, i);
                }
            }
            c = c->next;
        }
    }

    if (a == 0 && b == 0) {
        list_appen(free_list, p, s);
    }
}

void *krealloc(void *p, size_t size)
{
    int s = 0;
    list *c = allocated_list;
    while (c != NULL) {
        for (int i = 0; i < c->size; i++) {
            if (c->nodes[i].location == p) {
                s = c->nodes[i].size;
            }
        }
        c = c->next;
    }
    if (!s) { return NULL; }
    void *new = kmalloc(size);
    memcpy(new, p, s);

    kfree(p);
    return new;
}

void g(int t_size, int b, int f, int fu)
{
    void *ptrs[t_size];
    for (int i = 0; i < t_size; i++) {
        if (f) {
            int n = 0;
            while ((n = rand()) < f || n > fu) {}
            ptrs[i] = kmalloc(n);
            //ptrs[i] = kmalloc(rand() % b);
        } else {
            ptrs[i] = kmalloc(rand() % b);
        }
    }

    int c = 0;
    int i;
    while (c < 100000) {
        i = rand() % (t_size - 1);
        if (ptrs[i] == NULL) {
            c++;
        } else {
            kfree(ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    for (int i = 0; i < t_size; i++) {
        if (ptrs[i] != NULL) {
            kfree(ptrs[i]);
        }
    }
debug_print();
}

void kmalloc_tests()
{
    srand(2);
    g(10, 1000000, 1000000, 2000000);
    //g(10000, 99999, 0, 0);
    g(25, 1000000, 1000000, 2000000);
    g(10000, 299999, 0, 0);
}
