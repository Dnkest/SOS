#include "serial.h"

#include <picoro/picoro.h>
#include <stdio.h>

static serial_t sHandle;

static char *read_vaddr;
static int offset;

void serial_driver_init()
{
    sHandle = serial_init();
}

int serial_write(char *msg, int len)
{
    return serial_send(sHandle, msg, len);
}

void serial_read_handler(serial_t serial, char c)
{
    read_vaddr[offset++] = c;
}

int serial_read(char *buf, int len)
{
    read_vaddr = buf;
    offset = 0;

    serial_register_handler(sHandle, serial_read_handler);

    while (read_vaddr[offset-1] != '\n' && offset < len) {
        yield(0);
    }

    return offset;
}
