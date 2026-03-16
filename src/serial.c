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

void serial_read_str(char *buffer)
{
    size_t index = 0;
    const size_t max_len = 255;

    while (1) {
        uint8_t character = serial_read();

        if (character == '\r' || character == '\n') {
            serial_write('\r');
            serial_write('\n');
            break;
        }

        if ((character == '\b' || character == 127) && index > 0) {
            serial_write('\b');
            serial_write(' ');
            serial_write('\b');
            index--;
            buffer[index] = '\0';
            continue;
        }

        if (index < max_len) {
            serial_write(character);
            buffer[index++] = (char)character;
            buffer[index] = '\0';
        }
    }

    buffer[index] = '\0';
}
