#include <stdio.h>

#include "kmalloc.h"
#include "../frame_table.h"

#define CAPACITY ((1<<12) - sizeof(struct _list *) - sizeof(int))/sizeof(node)

static int t1 = 0;

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
    list *new_list = (list *)alloc_one_page();
    new_list->size = 0;
    new_list->next = NULL;
    return new_list;
}

void list_appen(list *l, node n)
{
    while (l->size == CAPACITY) {
        l = l->next;
    }
    l->nodes[l->size++] = n;
    if (l->size == CAPACITY) {
        l->next = list_ini();
    }
}

node list_find(list *l, void *p, size_t size) {
    list *cur = l;
    while (cur != NULL) {
        for (int i = cur->size-1; i >= 0; i--) {
            node tmp = cur->nodes[i];
            if ((size != 0 && tmp.size >= size) || (p != NULL && tmp.location == p)) {
                return tmp;
            }
        }
        cur = cur->next;
    }
    node not_found = { .location = 0,
                        .size = 0 };
    return not_found;
}

node list_find_end(list *l, void *p) {
    list *cur = l;
    while (cur != NULL) {
        for (int i = cur->size-1; i >= 0; i--) {
            node tmp = cur->nodes[i];
            if (tmp.location + tmp.size == p) {
                return tmp;
            }
        }
        cur = cur->next;
    }
    node not_found = { .location = 0,
                        .size = 0 };
    return not_found;
}

int list_del_index(list *cur, int i)
{
    cur->size--;
    for (int n = i; n < cur->size; n++) {
        cur->nodes[n] = cur->nodes[n+1];
    }

    if (cur->size == CAPACITY - 1 && cur->next != NULL) {
        list *tl = cur->next;
        while (tl->next != NULL) { tl = tl->next; }
        cur->nodes[cur->size++] = tl->nodes[tl->size--];
    }
}

int list_del_node(list *cur, node n)
{
    while (cur != NULL) {
        for (int i = 0; i < cur->size; i++) {
            node tmp = cur->nodes[i];
            if (tmp.size == n.size && tmp.location == n.location) {
                list_del_index(cur, i);
            }
        }
        cur = cur->next;
    }
}

int list_update(list *cur, node n, void *p, size_t size)
{
    while (cur != NULL) {
        for (int i = 0; i < cur->size; i++) {
            node tmp = cur->nodes[i];
            if (tmp.size == n.size && tmp.location == n.location) {
                cur->nodes[i].location = p;
                cur->nodes[i].size = size;
            }
        }
        cur = cur->next;
    }
}

void *kmalloc(size_t size)
{
    if (free_list == NULL) {
        free_list = list_ini();
    }

    if (allocated_list == NULL) {
        allocated_list = list_ini();
    }

if (t1++ == 10) { debug_print(); t1 = 0; }

    node free_node = list_find(free_list, NULL, size);
    int found = (free_node.location != 0 && free_node.size != 0);
    if (found) {
        node new_alloc_node = { .location = free_node.location,
                          .size = size };
        list_appen(allocated_list, new_alloc_node);
        list_del_node(free_list, free_node);
        if (free_node.size != size) {
            node new_free_node = { .location = free_node.location + size,
                          .size = free_node.size - size };
            list_appen(free_list, new_free_node);
        }
        return free_node.location;
    } else {
        void *new_frame = alloc_one_page();
        node new_node = { .location = new_frame, .size = (1<<12)};
        list_appen(free_list, new_node);
        return kmalloc(size);
    }
}

void debug_print_l(list *cur)
{
    while (cur != NULL) {
        for (int i = 0; i < cur->size; i++) {
            printf("%d:[%p, %lu] ", i, cur->nodes[i].location, cur->nodes[i].size);
        }
        cur = cur->next;
        printf("\n");
    }
}

void debug_print()
{
    // printf("\n====================================================================\n");
    // debug_print_l(allocated_list);
    // debug_print_l(free_list);
    // printf("====================================================================\n");
}


void kfree(void *p)
{
    if (allocated_list == NULL || free_list == NULL) {
        return;
    }

    node target_node = list_find(allocated_list, p, 0);
    int found = (target_node.location != 0 && target_node.size != 0);
    if (!found) { return; }

    memset(target_node.location, 0, target_node.size);
    list_del_node(allocated_list, target_node);
    list_appen(free_list, target_node);

    node f_merged_node = list_find(free_list, p + target_node.size, 0);
    found = (f_merged_node.location != 0 && f_merged_node.size != 0);
    if (found) {
        list_update(free_list, f_merged_node, target_node.location, f_merged_node.size + target_node.size);
        list_del_node(free_list, target_node);
    }
    node b_merged_node = list_find_end(free_list, p);
    f_merged_node = list_find(free_list, p, 0);

    found = (b_merged_node.location != 0 && b_merged_node.size != 0);
    if (found) {
        list_update(free_list, b_merged_node, b_merged_node.location, b_merged_node.size + f_merged_node.size);
        list_del_node(free_list, f_merged_node);
    }
}

void kmalloc_tests()
{
    void *p1 = kmalloc(10);
//debug_print();
    void *p2 = kmalloc(20);
    assert(p1 + 10 == p2);
//debug_print();
    void *p3 = kmalloc(4096);
//debug_print();
    void *p4 = kmalloc(96);
//debug_print();

    kfree(p2);
//debug_print();
    void *p5 = kmalloc(20);
    //assert(p2 == p5);


    
debug_print();
    kfree(p3);
debug_print();
    kfree(p1);
debug_print();
    kfree(p5);
    debug_print();

    kfree(p4);
    

    
debug_print();
}