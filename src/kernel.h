#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#define DEFAULT_COLOR_SCHEME ((ColorScheme){ .fg = 0xFFFFFFFF, .bg = 0xFF })
#define DEFAULT_CURSOR ((TTYPoint){.x = 0, .y = 0})

static void halt(void);
void command_eval();

// typedef struct {
//     uint8_t x;
//     uint8_t y;
// } TTYPoint;

// typedef struct {
//     TTYPoint position;
// } TTYCursor;

// typedef struct {
//     uint32_t fg;
//     uint32_t bg;
// } ColorScheme;

// typedef struct {
//     char character;
//     ColorScheme color;
// } TTYCell;

// typedef struct {
//     struct limine_framebuffer *framebuffer;
//     TTYCell buffer[25][80];
//     TTYPoint cursor;
//     int8_t width;
//     int8_t height;
//     ColorScheme color_scheme;
// } TTYEnvironment;

// void draw_pixel(uint64_t x, uint64_t y, uint32_t color);
// void draw_char(uint64_t x, uint64_t y, uint8_t c);
// void tty_cursor(uint8_t x, uint8_t y);
// void tty_char(uint8_t x, uint8_t y, uint8_t c);
// void tty_put_char(char str);
// void tty(TTYEnvironment *env);


uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0"
                    : "=a"(ret)
                    : "Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

#endif
