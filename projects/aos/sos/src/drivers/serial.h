#pragma once

#include <serial/serial.h>

typedef struct serial *serial_t;
typedef void (*handler_t)(serial_t serial, char c);

void serial_driver_init();
int serial_write(char *msg, int len);
int serial_read(char *buf, int len);
