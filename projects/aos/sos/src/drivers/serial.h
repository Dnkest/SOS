#pragma once

#include <serial/serial.h>

typedef struct serial *serial_t;
typedef void (*handler_t)(serial_t serial, char c);

void serial_driver_init();
void serial_write(char *msg, int len);
void serial_register_read_handler(handler_t read_handler);
