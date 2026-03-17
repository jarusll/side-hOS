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

static FreeList FreeListContext = {0}; // stores physical addr
static Command CommandContext = {0};
static uint64_t HHDM_OFFSET = {0};
static uint64_t PML4[512] __attribute__((aligned(4096))) = {0};

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

    // SETUP PAGING STRUCTURES
    // Using HHDM offset to map physical pages that are 4kb aligned
    // uint64_t pml4_entry, pdpt_entry, pd_entry, pt_entry;
    // uint64_t *pdpt, *pd, *pt;
    // for (int8_t i = 0; i < FreeListContext.cursor; i++){
    //     Segment virtual_segment = FreeListContext.segments[i];
    //     for (int64_t j = 0; j < virtual_segment.length; j++){
    //         uint64_t virtual_segment_start = virtual_segment.base + (j * SIZE_4KB);
    //         uint64_t pml4_index = (virtual_segment_start & PAGING_PML4_MASK) >> PAGING_PML4_SHIFT;
    //         pml4_entry = PML4[pml4_index];

    //         if ((pml4_entry & (1ULL << PAGING_PRESENT_FLAG)) == 0){
    //             // set the pml4 entry
    //             pml4_entry |= (1ULL << PAGING_PRESENT_FLAG); // always in ram, no demand paging
    //             pml4_entry |= (1ULL << PAGING_READ_WRITE_FLAG);
    //             pml4_entry |= (0ULL << PAGING_USER_SUPERVISOR_FLAG); // only supervisor page
    //             pml4_entry |= (0ULL << PAGING_PAGE_LEVEL_WRITE_THROUGH_FLAG); // use write back
    //             pml4_entry |= (0ULL << PAGING_PAGE_LEVEL_CACHE_DISABLE_FLAG); // cacheable
    //             pml4_entry |= (0ULL << PAGING_ACCESSED_FLAG); // cpu sets it
    //             pml4_entry |= (0b000ULL << PAGING_AVAILABLE_FLAG); // ignored
    //             uint64_t pdpt_physical = alloc_frame();
    //             pdpt = physical_to_virtual(pdpt_physical);
    //             pml4_entry |= (pdpt_physical & PAGING_NEXT_BASE_ADDRESS_MASK);
    //             PML4[pml4_index] = pml4_entry;
    //         } else {
    //             pdpt = physical_to_virtual(pml4_entry & PAGING_NEXT_BASE_ADDRESS_MASK);
    //         }

    //         uint64_t pdpt_index = (virtual_segment_start & PAGING_PDPT_MASK) >> PAGING_PDPT_SHIFT;
    //         pdpt_entry = pdpt[pdpt_index];
    //         if ((pdpt_entry & (1ULL << PAGING_PRESENT_FLAG)) == 0){
    //             // set the pdpt entry
    //             pdpt_entry |= (1ULL << PAGING_PRESENT_FLAG); // always in ram, no demand paging
    //             pdpt_entry |= (1ULL << PAGING_READ_WRITE_FLAG);
    //             pdpt_entry |= (0ULL << PAGING_USER_SUPERVISOR_FLAG); // only supervisor page
    //             pdpt_entry |= (0ULL << PAGING_PAGE_LEVEL_WRITE_THROUGH_FLAG); // use write back
    //             pdpt_entry |= (0ULL << PAGING_PAGE_LEVEL_CACHE_DISABLE_FLAG); // cacheable
    //             pdpt_entry |= (0ULL << PAGING_ACCESSED_FLAG); // cpu sets it
    //             pdpt_entry |= (0ULL << 7);
    //             pdpt_entry |= (0b000ULL << PAGING_AVAILABLE_FLAG); // ignored
    //             uint64_t pd_physical = alloc_frame();
    //             pd = physical_to_virtual(pd_physical);
    //             pdpt_entry |= (pd_physical & PAGING_NEXT_BASE_ADDRESS_MASK);
    //             pdpt[pdpt_index] = pdpt_entry;
    //         } else {
    //             pd = physical_to_virtual(pdpt_entry & PAGING_NEXT_BASE_ADDRESS_MASK);
    //         }

    //         uint64_t pd_index = (virtual_segment_start & PAGING_PD_MASK) >> PAGING_PD_SHIFT;
    //         pd_entry = pd[pd_index];
    //         if ((pd_entry & (1ULL << PAGING_PRESENT_FLAG)) == 0){
    //             // set the pd entry
    //             pd_entry |= (1ULL << PAGING_PRESENT_FLAG); // always in ram, no demand paging
    //             pd_entry |= (1ULL << PAGING_READ_WRITE_FLAG);
    //             pd_entry |= (0ULL << PAGING_USER_SUPERVISOR_FLAG); // only supervisor page
    //             pd_entry |= (0ULL << PAGING_PAGE_LEVEL_WRITE_THROUGH_FLAG); // use write back
    //             pd_entry |= (0ULL << PAGING_PAGE_LEVEL_CACHE_DISABLE_FLAG); // cacheable
    //             pd_entry |= (0ULL << PAGING_ACCESSED_FLAG); // cpu sets it
    //             pd_entry |= (0ULL << 7);
    //             pd_entry |= (0b000ULL << PAGING_AVAILABLE_FLAG); // ignored
    //             uint64_t pt_physical = alloc_frame();
    //             pt = physical_to_virtual(pt_physical);
    //             pd_entry |= (pt_physical & PAGING_NEXT_BASE_ADDRESS_MASK);
    //             pd[pd_index] = pd_entry;
    //         } else {
    //             pt = physical_to_virtual(pd_entry & PAGING_NEXT_BASE_ADDRESS_MASK);
    //         }

    //         uint64_t pt_index = (virtual_segment_start & PAGING_PT_MASK) >> PAGING_PT_SHIFT;
    //         pt_entry = pt[pt_index];
    //         if ((pt_entry & (1ULL << PAGING_PRESENT_FLAG)) == 0){
    //             // set the pt entry
    //             pt_entry |= (1ULL << PAGING_PRESENT_FLAG); // always in ram, no demand paging
    //             pt_entry |= (1ULL << PAGING_READ_WRITE_FLAG);
    //             pt_entry |= (0ULL << PAGING_USER_SUPERVISOR_FLAG); // only supervisor page
    //             pt_entry |= (0ULL << PAGING_PAGE_LEVEL_WRITE_THROUGH_FLAG); // use write back
    //             pt_entry |= (0ULL << PAGING_PAGE_LEVEL_CACHE_DISABLE_FLAG); // cacheable
    //             pt_entry |= (0ULL << PAGING_ACCESSED_FLAG); // cpu sets it
    //             pt_entry |= (0ULL << PAGING_DIRTY_FLAG); // ignored
    //             pt_entry |= (0ULL << PAGING_PAGE_SIZE_FLAG); // 4kb pages, ignored
    //             pt_entry |= (1ULL << PAGING_GLOBAL_PAGE_FLAG); // single address space
    //             pt_entry |= (0b000ULL << PAGING_AVAILABLE_FLAG); // ignored
    //             uint64_t page_physical = alloc_frame();
    //             pt_entry |= (page_physical & PAGING_NEXT_BASE_ADDRESS_MASK);
    //             pt[pt_index] = pt_entry;
    //         }
    //     }
    // }

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
        uint64_t frame = alloc_frame();
        puthex(frame);
        kstdout("\r\n");
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
            memset(physical_to_virtual(frame), 0, 4096);
            return frame;
        }
        segment_cursor++;
    }

    return 0;
}

uint64_t memory_nth_segment(Segment *seg, uint64_t n)
{
    return seg->base + (SIZE_4KB * n);
}
