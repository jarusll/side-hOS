#include "kernel.h"
#include "serial.h"

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

// static TTYEnvironment env = {0};

typedef struct {
    char command[256];
    int8_t cursor;
} Command;

static Command CommandContext = {0};

void kmain(void) {
    // Ensure the bootloader actually understands our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

    // env.cursor = DEFAULT_CURSOR;
    // env.color_scheme = DEFAULT_COLOR_SCHEME;
    // env.width = 80;
    // env.height = 25;
    // env.buffer[0][0].character = (uint8_t)'>';
    // env.cursor = (TTYPoint){
    //     .x = 1,
    //     .y = 0
    // };

    // Ensure we got a framebuffer.
    // if (framebuffer_request.response == NULL
    //     || framebuffer_request.response->framebuffer_count < 1) {
    //     halt();
    // }
    // env.framebuffer = framebuffer_request.response->framebuffers[0];

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

    serial_init();
    while (1){
        serial_write('>');
waitForInput:
        while(!serial_received());
        while (serial_received()){
            uint8_t character = serial_read();
            if (character == 0x08 || character == 0x7F) {   // backspace
                serial_write('\b');
                serial_write(' ');
                serial_write('\b');
                CommandContext.cursor--;
                CommandContext.command[CommandContext.cursor] = '\0';
            } else if (character == '\r' || character == '\n') {
                serial_write('\r');
                serial_write('\n');
                command_eval();
                serial_write('\r');
                serial_write('\n');
                serial_write('>');
            } else {
                serial_write(character);
                CommandContext.command[CommandContext.cursor] = character;
                CommandContext.cursor++;
                CommandContext.command[CommandContext.cursor] = '\0';
            }
            goto waitForInput;
        }
    }

    // TODO
    // hhdm_request

    halt();
}

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

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
}


void command_eval(){
    if (strcmp(CommandContext.command, "ping") == 0){
        char *str = "pong";
        serial_write_str(str);
    } else {
        serial_write_str("Invalid command");
    }

    CommandContext = (Command){0};
}
