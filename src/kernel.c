#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "font8x16.h"

#define DEFAULT_COLOR_SCHEME ((ColorScheme){ .fg = 0xFFFFFFFF, .bg = 0xFF })
#define DEFAULT_CURSOR ((TTYPoint){.x = 0, .y = 0})

// LIMINE Boot Protocol Specification: https://codeberg.org/Limine/limine-protocol/src/branch/trunk/PROTOCOL.md
// See linker script for how these are placed in the binary - ../linker.lds

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// GCC wants these 4 functions
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

static void halt(void) {
    for (;;) {
        asm ("hlt");
    }
}

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

void draw_pixel(uint64_t x, uint64_t y, uint32_t color, struct limine_framebuffer *framebuffer);
void draw_char(uint64_t x, uint64_t y, uint8_t c, struct limine_framebuffer *framebuffer);
void tty_cursor(uint8_t x, uint8_t y, TTYEnvironment *env);
void tty_char(uint8_t x, uint8_t y, uint8_t c, TTYEnvironment *env);
void tty(TTYEnvironment *env);

void kmain(void) {
    // Ensure the bootloader actually understands our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

    static TTYEnvironment env = {0};
    env.cursor = DEFAULT_CURSOR;
    env.color_scheme = DEFAULT_COLOR_SCHEME;
    env.width = 80;
    env.height = 25;
    env.buffer[0][0].character = (uint8_t)'>';
    env.cursor = (TTYPoint){
        .x = 1,
        .y = 0
    };

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
        || framebuffer_request.response->framebuffer_count < 1) {
        halt();
    }
    env.framebuffer = framebuffer_request.response->framebuffers[0];

    tty(&env);
    // See ../limine.conf for resolution and bits per pixel settings
    // for (uint64_t y = 0; y < env.framebuffer->height; y++) {
    //     for (uint64_t x = 0; x < env.framebuffer->width; x++) {
    //         uint32_t color = 0xFF << env.framebuffer->red_mask_shift;
    //         draw_pixel(x, y, color, env.framebuffer);
    //     }
    // }

    halt();
}

void tty(TTYEnvironment *env){
    while(1){
        // draw buffer
        for (int8_t x = 0; x < env->width; x++){
            for (int8_t y = 0; y < env->height; y++){
                tty_char(x, y, env->buffer[y][x].character, env);
            }
        }

        // draw cursor
        tty_cursor(env->cursor.x, env->cursor.y, env);
        while(1);
    }
}

void tty_char(uint8_t x, uint8_t y, uint8_t c, TTYEnvironment *env){
    for (uint64_t j = 0; j < 16; j++) {
        uint8_t row = font8x16[c][j];
        for (uint64_t i = 0; i < 8; i++) {
            uint8_t pixel = row & (1 << (7 - i));
            draw_pixel(x * 8 + i, y * 16 + j, pixel ? env->color_scheme.fg : env->color_scheme.bg, env->framebuffer);
        }
    }
}

void tty_cursor(uint8_t x, uint8_t y, TTYEnvironment *env){
    for (uint64_t j = 0; j < 16; j++) {
        uint8_t row = cursor8x16[j];
        for (uint64_t i = 0; i < 8; i++) {
            uint8_t pixel = row & (1 << (7 - i));
            draw_pixel(x * 8 + i, y * 16 + j, pixel ? env->color_scheme.fg : env->color_scheme.bg, env->framebuffer);
        }
    }
}

void draw_pixel(uint64_t x, uint64_t y, uint32_t color, struct limine_framebuffer *framebuffer) {
    volatile uint8_t *fb_ptr = framebuffer->address;
    *(uint32_t*)(fb_ptr + y * framebuffer->pitch + x * (framebuffer->bpp / 8)) = color;
}

void draw_char(uint64_t x, uint64_t y, uint8_t c, struct limine_framebuffer *framebuffer) {
    uint32_t white = 0xFFFFFFFF;
    uint32_t black = 0xFF << framebuffer->red_mask_shift;
    for (uint64_t j = 0; j < 16; j++) {
        uint8_t row = font8x16[c][j];
        for (uint64_t i = 0; i < 8; i++) {
            uint8_t pixel = row & (1 << i);
            draw_pixel(x * 8 + i, y * 16 + j, pixel ? white : black, framebuffer);
        }
    }
}

// void stdio(char *str) {

// }
