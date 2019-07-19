#include "fd_table.h"
#include "../utils/kmalloc.h"
#include "../utils/idalloc.h"

fd_table_t *fdt_init()
{
    fd_table_t *t = kmalloc(sizeof(fd_table_t));
    t->idalloc = id_table_init(3, 1, 16);
}

int fdt_insert(fd_table_t *table, const char *path, size_t size, vnode_t *vnode, fmode_t mode)
{
    int id = (int)id_alloc(table->idalloc, 1);
    fd_entry_t *ent = kmalloc(sizeof(fd_entry_t));
    ent->mode = mode;
    ent->size = size;
    ent->path = path;
    ent->offset = 0;
    ent->vnode = vnode;
    table->entries[id] = ent;
    return id;
}

int fdt_delete(fd_table_t *table, int file)
{
    fd_entry_t *e = table->entries[file];
    if (!e) { return -1; }

    kfree(e->vnode);
    kfree(e);
    id_free(table->idalloc, file, 1);
    return 0;
}

int fdt_entry_exists(fd_table_t *table, int file)
{
    return (table->entries[file] != NULL);
}
