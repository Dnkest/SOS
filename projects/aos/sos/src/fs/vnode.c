#include "vnode.h"
#include "../utils/kmalloc.h"

vnode_t *vnode_init(vnode_close close_f,
    vnode_read read_f,
    vnode_write write_f)
{
    vnode_t *vnode = kmalloc(sizeof(vnode_t));
    vnode->read_f = read_f;
    vnode->close_f = close_f;
    vnode->write_f = write_f;
    vnode->fh = NULL;
    return vnode;
}
