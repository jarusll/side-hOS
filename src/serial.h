#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define COM1 0x3F8

uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t val);

void serial_init();
int serial_can_send();
int serial_received();
void serial_write(uint8_t c);
uint8_t serial_read();
void serial_write_str(char*);

#endif
