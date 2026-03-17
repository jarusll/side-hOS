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
    char command[UINT8_MAX];
    int8_t cursor;
} Command;

static FreeList FreeListContext = {0}; // stores physical addr
static Command CommandContext = {0};
static uint64_t HHDM_OFFSET = {0};
static uint64_t PML4[512] __attribute__((aligned(4096))) = {0};
static uint64_t frame;

static inline void init_hardware(void) {
    serial_init();
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
    // get the hhdm offset
    HHDM_OFFSET = hhdm_reqest.response->offset;

    init_hardware();

    kstdout("Hello, World\n");

    while (1){
        kstdout(">");
        kstdin();
        kshell();
        kstdout("\r\n");
    }

    halt();
}

void kshell(){
    if (strcmp(CommandContext.command, "ping") == 0){
        char *str = "pong";
        kstdout(str);
    } else if(strcmp(CommandContext.command, "alloc") == 0){
        frame = alloc_frame();
        puthex(frame);
        kstdout("\r\n");
    } else if(strcmp(CommandContext.command, "free") == 0){
        bool status = free_frame(frame);
        puthex(frame);
        kstdout("\n");
        if (status){
            kstdout("freed\n");
        } else {
            kstdout("error\n");
        }
    } else if(strcmp(CommandContext.command, "freelist") == 0){
        kstdout("cursor=");
        puthex(FreeListContext.cursor);
        kstdout("\r\n");

        for (int i = 0; i < FreeListContext.cursor; i++){
            kstdout("seg ");
            puthex(i);
            kstdout(" base=");
            puthex(FreeListContext.segments[i].base);
            kstdout(" len=");
            puthex(FreeListContext.segments[i].length);
            kstdout("\r\n");
        }
    } else if(strcmp(CommandContext.command, "hhdm") == 0){
        kstdout("hhdm=");
        puthex(HHDM_OFFSET);
        kstdout("\r\n");
    } else {
        kstdout("Invalid command");
    }

    CommandContext = (Command){0};
}

void kstdout(char* str){
    serial_write_str(str);
}

void puthex(uint64_t value)
{
    serial_write('0');
    serial_write('x');

    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4){
        uint8_t nibble = (value >> shift) & 0xF;

        if (!started){
            if (nibble == 0 && shift != 0){
                continue;
            }
            started = true;
        }

        serial_write((nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10));
    }

    if (!started){
        serial_write('0');
    }
}

char* kstdin(){
    serial_read_str(CommandContext.command);
    return CommandContext.command;
}

uint64_t virtual_to_physical(uint64_t virtual){
    return virtual - HHDM_OFFSET;
}

uint64_t* physical_to_virtual(uint64_t physical){
    return (uint64_t*)(physical + HHDM_OFFSET);
}

uint64_t alloc_frame()
{
    uint8_t segment_cursor = 0;
    while (segment_cursor < FreeListContext.cursor){
        Segment *segment = &FreeListContext.segments[segment_cursor];
        if (segment->length > 0){
            uint64_t frame = memory_nth_segment(segment, segment->length - 1);
            segment->length--;
            // zero out
            memset(physical_to_virtual(frame), 0, SIZE_4KB);
            return frame;
        }
        segment_cursor++;
    }

    return 0;
}

bool free_frame(uint64_t physical)
{
    uint8_t segment_cursor = 0;
    while (segment_cursor < FreeListContext.cursor){
        Segment *segment = &FreeListContext.segments[segment_cursor];
        // end adjacent
        if ((segment->base + segment->length * SIZE_4KB) == physical){
            segment->length++;
            return true;
        }
        // start adjacent
        if (physical + SIZE_4KB == segment->base){
            segment->length++;
            segment->base = physical;
            return true;
        }
        segment_cursor++;
    }
    if (FreeListContext.cursor == UINT8_MAX){
        return false;
    }
    // new segment
    FreeListContext.segments[FreeListContext.cursor] = (Segment){
        .base = physical,
        .length = 1
    };
    FreeListContext.cursor++;
    return true;
}

uint64_t memory_nth_segment(Segment *seg, uint64_t n)
{
    return seg->base + (SIZE_4KB * n);
}

uint64_t* kmalloc(uint64_t size)
{
    return NULL;
}

bool kfree(uint64_t){
    return false;
}
