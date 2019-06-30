#include <stdio.h>
#include <stdlib.h>
#include <adt/linkedlist.h>

struct _it {
    void *data;
    struct _it *next;
};

typedef struct _list {
    Iterator head;
} LinkedList;

Iterator it_create(void *data)
{
    Iterator it = malloc(sizeof(struct _it));
    it->data = data;
    it->next = NULL;

    return it;
}

List list_create()
{
    List list = (List) malloc(sizeof(LinkedList));
    list->head = NULL;
    return list;
}

Iterator list_iterator(List list)
{
    return list->head;
}

int it_has_next(Iterator it)
{
    return (it != NULL);
}

void *it_next(Iterator *it)
{
    void *ret = (*it)->data;
    *it = (*it)->next;
    return ret;
}

void *it_peek(Iterator it)
{
    return it->data;
}

void list_pushback(List list, void *data)
{
    Iterator it = it_create(data);
    if (list->head == NULL) {
        list->head = it;
    } else {
        Iterator cur = list->head;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = it;
    }
}

void list_delete(List list, void *data, comparsion p)
{
    if (list->head == NULL) {
        return;
    }

    if (p(list->head->data, data)) {
        list->head = list->head->next;
        free(list->head);
        return;
    }

    Iterator cur = list->head, prev = NULL;
    while (cur) {
        prev = cur;
        cur = cur->next;
        if (p(cur->data, data)) {
            prev->next = cur->next;
            free(cur);
        }
    }
}