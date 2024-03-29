#pragma once

#include <stdlib.h>

#include "fs_types.h"
#include "fd_table.h"
#include "vnode.h"

int sos_nfs_open(struct nfsfh **fh, const char *path, int flags);
int sos_nfs_close(struct nfsfh *fh);
int sos_nfs_write(struct nfsfh *fh, const char *buf, size_t offset, size_t nbyte);
int sos_nfs_read(struct nfsfh *fh, char *buf, size_t offset, size_t nbyte);
int sos_nfs_getdirent(int pos, char *name, size_t nbyte);
int sos_nfs_stat(const char *path, sos_stat_t *buf);
void sos_nfs_lseek(struct nfsfh *fh, size_t offset);

void sos_nfs_set_context(struct nfs_context *context);
struct nfs_context *sos_nfs_get_context();
