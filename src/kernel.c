#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "font8x16.h"

void draw_pixel(uint64_t x, uint64_t y, uint32_t color, struct limine_framebuffer *framebuffer);
void draw_char(uint64_t x, uint64_t y, uint8_t c, struct limine_framebuffer *framebuffer);

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
    struct limine_framebuffer *framebuffer;
    int8_t width;
    int8_t height;
} Environment;

void kmain(void) {
    // Ensure the bootloader actually understands our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

    static Environment env = {0};
    env.framebuffer = NULL;
    env.width = 80;
    env.height = 25;

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
        || framebuffer_request.response->framebuffer_count < 1) {
        halt();
    }
    env.framebuffer = framebuffer_request.response->framebuffers[0];

    // See ../limine.conf for resolution and bytes per pixel settings
    for (uint64_t y = 0; y < env.framebuffer->height; y++) {
        for (uint64_t x = 0; x < env.framebuffer->width; x++) {
            uint32_t color = 0xFF << env.framebuffer->red_mask_shift;
            draw_pixel(x, y, color, env.framebuffer);
        }
    }

    draw_char(0, 0, 65, env.framebuffer);

    halt();
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
