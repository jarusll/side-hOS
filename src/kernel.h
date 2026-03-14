#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "font8x16.h"

#define DEFAULT_COLOR_SCHEME ((ColorScheme){ .fg = 0xFFFFFFFF, .bg = 0xFF })
#define DEFAULT_CURSOR ((TTYPoint){.x = 0, .y = 0})

static void halt(void);

typedef struct {
    uint8_t x;
    uint8_t y;
} TTYPoint;

typedef struct {
    TTYPoint position;
} TTYCursor;

typedef struct {
    uint32_t fg;
    uint32_t bg;
} ColorScheme;

typedef struct {
    char character;
    ColorScheme color;
} TTYCell;

typedef struct {
    struct limine_framebuffer *framebuffer;
    TTYCell buffer[25][80];
    TTYPoint cursor;
    int8_t width;
    int8_t height;
    ColorScheme color_scheme;
} TTYEnvironment;

void draw_pixel(uint64_t x, uint64_t y, uint32_t color);
void draw_char(uint64_t x, uint64_t y, uint8_t c);
void tty_cursor(uint8_t x, uint8_t y);
void tty_char(uint8_t x, uint8_t y, uint8_t c);
void tty_put_char(char str);
void tty(TTYEnvironment *env);

// GCC wants these 4 functions, ignore the squigglies in vscode
void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if ((uintptr_t)src > (uintptr_t)dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if ((uintptr_t)src < (uintptr_t)dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}
