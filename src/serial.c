#include "serial.h"

void serial_init() {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

int serial_can_send() {
    return inb(COM1 + 5) & 0x20;
}

int serial_received() {
    return inb(COM1 + 5) & 1;
}

void serial_write(uint8_t c) {
    while (!serial_can_send());
    outb(COM1, c);
}

uint8_t serial_read() {
    while (!serial_received());
    return inb(COM1);
}

void serial_write_str(char *s)
{
    int8_t index = 0;
    while (s[index] != '\0'){
        serial_write(s[index++]);
    }
}
