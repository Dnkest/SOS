#include <picoro/picoro.h>
#include <serial/serial.h>
#include <stdio.h>

#include "serial.h"

static struct serial *sHandle = NULL;
static int occupied = 0;

static char *read_vaddr;
static size_t offset;

static void serial_read_handler(struct serial *serial, char c)
{
    read_vaddr[offset++] = c;
}

int serial_open(int flags)
{
    if (sHandle == NULL) {
        sHandle = serial_init();
        if (sHandle == NULL) {
            return -1;
        }
        serial_register_handler(sHandle, serial_read_handler);
        occupied = 1;
    } else {
        // if (flags != 1) {
        //     while (occupied) { yield(NULL); }
        //     occupied = 1;
        // }
        /* TODO: waits for console. */
        return -1;
    }
    return 0;
}

int serial_write(struct nfsfh *fh, char *msg, size_t offset, size_t len)
{
    return serial_send(sHandle, msg, (int)len);
}

int serial_read(struct nfsfh *fh, char *buf, size_t unused, size_t len)
{
    read_vaddr = buf;
    offset = 0;

    while (read_vaddr[offset-1] != '\n' && offset < len) {
        yield(0);
    }
    return offset;
}

int serial_close(struct nfsfh *fh)
{
    occupied = 0;
    return 0;
}
