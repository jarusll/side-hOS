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

// static TTYEnvironment env = {0};

typedef struct {
    char command[256];
    int8_t cursor;
} Command;

static FreeList FreeListContext = {0};
static Command CommandContext = {0};

static inline void init_hardware(void) {
    serial_init();
}

static inline uint64_t next_4kb(uint64_t addr) {
    const uint64_t page_mask = 0xFFF;
    return (addr + page_mask) & ~page_mask;
}

void kmain(void) {
    // Ensure the bootloader actually understands our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

    // get all memory segments
    if (memmap_request.response == NULL
        || memmap_request.response->entry_count <= 0) {
        halt();
    }
    // get usable memory segments
    uint64_t entry_count = memmap_request.response->entry_count;
    for (uint64_t i = 0; i < entry_count; i++){
        struct limine_memmap_entry entry = *memmap_request.response->entries[i];
        if (entry.type == LIMINE_MEMMAP_USABLE){
            uint64_t aligned_start = (entry.base + 0xfff) & ~0xfff;
            uint64_t segment_end = entry.base + entry.length - 1;
            uint64_t aligned_end = (segment_end + 0xfff) & ~0xfff;
            if (aligned_start != aligned_end){
                FreeListContext.segments[FreeListContext.cursor] = (Segment){
                    .base = aligned_start,
                    .length = (aligned_end - aligned_start) >> 12
                };
                FreeListContext.cursor++;
            }
        }
    }

    // 4 level paging
    if (paging_request.response == NULL){
        halt();
    }

    // make life easy by linear mapping with an offset
    if (hhdm_reqest.response == NULL){
        halt();
    }

    // SETUP PAGING STRUCTURES
    // Using HHDM mapping physical pages that are 4kb aligned


    init_hardware();

    kstdout("Hello, World\n");

    while (1){
        kstdout(">");
        kstdin();
        kshell();
        kstdout("\r\n");
    }

    // TODO
    // hhdm_request

    halt();
}

void kshell(){
    if (strcmp(CommandContext.command, "ping") == 0){
        char *str = "pong";
        kstdout(str);
    } else {
        kstdout("Invalid command");
    }

    CommandContext = (Command){0};
}

void kstdout(char* str){
    serial_write_str(str);
}

char* kstdin(){
    serial_read_str(CommandContext.command);
    return CommandContext.command;
}
