#include "serial.h"

static serial_t sHandle;

void serial_driver_init()
{
    sHandle = serial_init();
}

void serial_write(char *msg, int len)
{
    serial_send(sHandle, msg, len);
}

void serial_register_read_handler(handler_t read_handler)
{
    serial_register_handler(sHandle, read_handler);
}
