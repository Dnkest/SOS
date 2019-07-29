#pragma once

#include <stdlib.h>

#include "fd_table.h"
#include "fs_types.h"
#include "vnode.h"

int serial_open(fmode_t mode);
int serial_write(struct nfsfh *fh, char *msg, size_t offset, size_t len);
int serial_read(struct nfsfh *fh, char *buf, size_t offset, size_t len);
int serial_close(struct nfsfh *fh);
