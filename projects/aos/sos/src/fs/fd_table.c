#include <string.h>

#include "fd_table.h"
#include "sos_nfs.h"
#include "../utils/kmalloc.h"
#include "../utils/low_avail_id.h"

struct fd_table {
    fd_entry_t *entries[MAX_FD];
    low_avail_id_t *ids;
};

fd_table_t *fdt_init()
{
    fd_table_t *t = kmalloc(sizeof(fd_table_t));
    t->ids = low_avail_id_init(3, 1, 16);
}

int fdt_insert(fd_table_t *table, const char *path, int flags, size_t size, vnode_t *vnode)
{
    int id = (int)low_avail_id_alloc(table->ids, 1);
    fd_entry_t *ent = kmalloc(sizeof(fd_entry_t));
    ent->flags = flags;
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
    low_avail_id_free(table->ids, file, 1);
    return 0;
}

fd_entry_t *fdt_get_entry(fd_table_t *table, int file)
{
    return table->entries[file];
}

void fdt_destroy(fd_table_t *table)
{
    vnode_t *vnode;
    for (int i = 0; i < MAX_FD; i++) {
        fd_entry_t *e = table->entries[i];
        if (e != NULL) {
            if (strcmp("console", e->path)) {
                vnode = e->vnode;
                sos_nfs_close(vnode->fh);
            }
            kfree(e->vnode);
            kfree(e);
        }
    }
    low_avail_id_destroy(table->ids);
    kfree(table);
}
