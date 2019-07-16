#pragma once

#include <stdlib.h>
#include "fs_types.h"

typedef int (*vnode_close)(struct nfsfh *fh);
typedef int (*vnode_read)(struct nfsfh *fh, char *buf, size_t offset, size_t nbyte);
typedef int (*vnode_write)(struct nfsfh *fh, const char *buf, size_t offset, size_t nbyte);

typedef struct vnode {
    struct nfsfh *fh;
    vnode_close close_f;
    vnode_read read_f;
    vnode_write write_f;
} vnode_t;

vnode_t *vnode_init(vnode_close close_f, vnode_read read_f, vnode_write write_f);
vnode_t *vnode_copy(vnode_t *vnode);
