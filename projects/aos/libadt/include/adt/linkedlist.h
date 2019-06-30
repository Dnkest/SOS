#pragma once

typedef struct _it *Iterator;
typedef struct _list *List;

typedef int (*comparsion)(void *, void *);

List list_create();
Iterator list_iterator(List list);

int it_has_next(Iterator it);
void *it_next(Iterator *it);
void *it_peek(Iterator it);

void list_pushback(List list, void *data);
void list_delete(List list, void *data, comparsion p);
