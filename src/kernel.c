#include "kernel.h"
// LIMINE Boot Protocol Specification: https://codeberg.org/Limine/limine-protocol/src/branch/trunk/PROTOCOL.md
// See linker script for how these are placed in the binary - ../linker.lds

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request paging_request = {
    .id = LIMINE_PAGING_MODE_REQUEST_ID,
    .revision = 0,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_reqest = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

// ENABLE MULTIPROCESSING
// __attribute__((used, section(".limine_requests")))
// static volatile struct limine_mp_request mp_request = {
//     .id = LIMINE_MP_REQUEST_ID,
//     .revision = 0,
//     .flags = 0x0000000000000001
// };

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void halt(void) {
    for (;;) {
        asm ("hlt");
    }
}

static TTYEnvironment env = {0};

void kmain(void) {
    // Ensure the bootloader actually understands our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

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

    // Ensure we got the memmap response
    if (memmap_request.response == NULL
        || memmap_request.response->entry_count <= 0) {
        halt();
    }

    if (paging_request.response == NULL){
        halt();
    }

    if (hhdm_reqest.response == NULL){
        halt();
    }

    // TODO
    // hhdm_request

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
                tty_char(x, y, env->buffer[y][x].character);
            }
        }

        // draw cursor
        tty_cursor(env->cursor.x, env->cursor.y);
        while(1);
    }
}

void tty_char(uint8_t x, uint8_t y, uint8_t c){
    for (uint64_t j = 0; j < 16; j++) {
        uint8_t row = font8x16[c][j];
        for (uint64_t i = 0; i < 8; i++) {
            uint8_t pixel = row & (1 << (7 - i));
            draw_pixel(x * 8 + i, y * 16 + j, pixel ? env.color_scheme.fg : env.color_scheme.bg);
        }
    }
}

void tty_put_char(char ch){
    tty_char(env.cursor.x, env.cursor.y, ch);
}

void tty_cursor(uint8_t x, uint8_t y){
    for (uint64_t j = 0; j < 16; j++) {
        uint8_t row = cursor8x16[j];
        for (uint64_t i = 0; i < 8; i++) {
            uint8_t pixel = row & (1 << (7 - i));
            draw_pixel(x * 8 + i, y * 16 + j, pixel ? env.color_scheme.fg : env.color_scheme.bg);
        }
    }
}

void draw_pixel(uint64_t x, uint64_t y, uint32_t color) {
    struct limine_framebuffer *framebuffer = env.framebuffer;
    volatile uint8_t *fb_ptr = framebuffer->address;
    *(uint32_t*)(fb_ptr + y * framebuffer->pitch + x * (framebuffer->bpp / 8)) = color;
}

void draw_char(uint64_t x, uint64_t y, uint8_t c) {
    uint32_t white = 0xFFFFFFFF;
    struct limine_framebuffer *framebuffer = env.framebuffer;
    uint32_t black = 0xFF << env.framebuffer->red_mask_shift;
    for (uint64_t j = 0; j < 16; j++) {
        uint8_t row = font8x16[c][j];
        for (uint64_t i = 0; i < 8; i++) {
            uint8_t pixel = row & (1 << i);
            draw_pixel(x * 8 + i, y * 16 + j, pixel ? white : black);
        }
    }
}

void stdio(char *str) {
    int64_t index = 0;
    while (str[index] != '\0'){
        tty_put_char(str[index]);
        index++;
    }
}
