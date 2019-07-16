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

vnode_t *vnode_copy(vnode_t *vnode)
{
    vnode_t *ret = kmalloc(sizeof(vnode_t));
    ret->fh = vnode->fh;
    ret->read_f = vnode->read_f;
    ret->close_f = vnode->close_f;
    ret->write_f = vnode->write_f;
    return ret;
}
