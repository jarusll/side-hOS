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

static MemoryContext Memory = {0};
static Command CommandContext = {0};
static uint64_t HHDM_OFFSET = {0};
static PointerList PointerStore = {0};
static uint64_t frame;
static uint64_t last = {0};

static bool pointer_push(PointerList *list, uint64_t *ptr)
{
    if (list->cursor == UINT8_MAX){
        return false;
    }
    list->entries[list->cursor++] = ptr;
    return true;
}

static uint64_t* pointer_pop(PointerList *list)
{
    if (list->cursor == 0){
        return NULL;
    }
    list->cursor--;
    return list->entries[list->cursor];
}

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
                Segment *segments = Memory.freelist.segments;
                int8_t *cursor = &Memory.freelist.cursor;
                segments[*cursor] = (Segment){
                    .base = aligned_start,
                    .length = (aligned_end - aligned_start) >> 12
                };
                // TODO - handle overflow
                (*cursor)++;
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
    } else if(strcmp(CommandContext.command, "malloc") == 0){
        uint64_t size = 1000;
        uint64_t *addr = kmalloc(size);
        kstdout("malloc size=");
        puthex(size);
        kstdout(" addr=");
        puthex((uint64_t)addr);
        kstdout("\r\n");

        if (addr){
            if (!pointer_push(&PointerStore, addr)){
                kstdout("pointer list full\r\n");
            } else {
                kstdout("malloc success\r\n");
            }
        } else {
            kstdout("malloc failed\r\n");
        }
    } else if(strcmp(CommandContext.command, "alloc") == 0){
        frame = alloc_frame();
        puthex(frame);
        kstdout("\r\n");
    } else if(strcmp(CommandContext.command, "free") == 0){
        uint64_t *addr = pointer_pop(&PointerStore);
        if (!addr){
            kstdout("no alloc to free\r\n");
        } else {
            bool status = kfree(addr);
            kstdout("free addr=");
            puthex((uint64_t)addr);
            kstdout(status ? " ok\r\n" : " fail\r\n");
        }
    } else if(strcmp(CommandContext.command, "freeframe") == 0){
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
        uint8_t *cursor = &Memory.freelist.cursor;
        puthex(*cursor);
        kstdout("\r\n");

        Segment *segments = Memory.freelist.segments;
        for (int i = 0; i < *cursor; i++){
            kstdout("seg ");
            puthex(i);
            kstdout(" base=");
            puthex(segments[i].base);
            kstdout(" len=");
            puthex(segments[i].length);
            kstdout("\r\n");
        }
    } else if(strcmp(CommandContext.command, "hhdm") == 0){
        kstdout("hhdm=");
        puthex(HHDM_OFFSET);
        kstdout("\r\n");
    } else if(strcmp(CommandContext.command, "heapdump") == 0){
        HeapPage *page = Memory.heap_pages;
        if (!page){
            kstdout("no heap pages\r\n");
        }

        uint64_t page_index = 0;
        while (page){
            kstdout("page ");
            puthex(page_index);
            kstdout(" base=");
            puthex((uint64_t)page);
            kstdout(" largest=");
            puthex(page->largest);
            kstdout("\r\n");

            HeapNode *node = page->freelist;
            uint64_t node_index = 0;
            while (node){
                kstdout("  node ");
                puthex(node_index);
                kstdout(" len=");
                puthex(node->length);
                kstdout(" next=");
                puthex((uint64_t)node->next);
                kstdout("\r\n");

                node = node->next;
                node_index++;
            }

            page = page->next;
            page_index++;
        }
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

uint64_t virtual_to_physical(uint64_t *virtual){
    return (uint64_t)virtual - HHDM_OFFSET;
}

uint64_t* physical_to_virtual(uint64_t physical){
    return (uint64_t*)(physical + HHDM_OFFSET);
}

uint64_t alloc_frame()
{
    uint8_t segment_cursor = 0;
    while (segment_cursor < Memory.freelist.cursor){
        Segment *segment = &Memory.freelist.segments[segment_cursor];
        if (segment->length > 0){
            uint64_t frame_physical = memory_nth_segment(segment, segment->length - 1);
            uint64_t *frame_pointer = physical_to_virtual(frame_physical);
            segment->length--;

            // zero out
            memset(frame_pointer, 0, SIZE_4KB);

            uint64_t free_size = SIZE_4KB - sizeof(HeapPage) - sizeof(HeapNode);
            HeapNode *node = (HeapNode*)(pointer(frame_pointer) + sizeof(HeapPage));
            node->length = free_size;
            node->next = NULL;

            HeapPage *page = (HeapPage*)frame_pointer;
            page->freelist = node;
            page->largest = free_size;

            page->next = Memory.heap_pages;
            Memory.heap_pages = page;

            return frame_physical;
        }
        segment_cursor++;
    }

    return 0;
}

bool free_frame(uint64_t physical)
{
    uint8_t segment_cursor = 0;
    uint8_t *cursor = &Memory.freelist.cursor;
    while (segment_cursor < *cursor){
        Segment *segment = &Memory.freelist.segments[segment_cursor];
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
    if (*cursor == UINT8_MAX){
        return false;
    }
    // new segment
    Segment *segments = Memory.freelist.segments;
    segments[*cursor] = (Segment){
        .base = physical,
        .length = 1
    };
    *cursor++;
    return true;
}

uint64_t memory_nth_segment(Segment *seg, uint64_t n)
{
    return seg->base + (SIZE_4KB * n);
}

uint64_t* kmalloc(uint64_t size)
{
    // ignore multi frame allocs
    if (size > SMALL_PAGE_MAXIMUM_SIZE) {
        return 0;
    }

    uint64_t total_size = size + sizeof(uint64_t);
    HeapPage *page = Memory.heap_pages;
    while (page && page->largest < total_size){
        page = page->next;
    }

    if (page == NULL || page->largest < total_size){
        uint64_t new_physical_frame = alloc_frame();
        page = (HeapPage*)physical_to_virtual(new_physical_frame);
    }

    // search for free heap space
    HeapNode *node = page->freelist;
    HeapNode *prev, *next;
    prev = NULL;
    while (node && node->length < total_size){
        prev = node;
        node = node->next;
    }

    if (!node){
        return 0;
    }
    next = node->next;

    // preserve node header
    uint64_t *base = (uint64_t*)(pointer(node) + sizeof(HeapNode));
    // write size header
    *base = size;
    uint64_t *return_base = (uint64_t*)(pointer(base) + sizeof(uint64_t));
    uint64_t *new_next = (uint64_t*)(pointer(base) + total_size);
    uint64_t remaining_size = node->length - total_size;

    HeapNode *replacement;
    if (remaining_size < sizeof(HeapNode)){ // cant split
        replacement = next;
        // update to use all remaining size
        *base = size + remaining_size;
    } else {
        // split and create a new free node
        HeapNode *new_node = (HeapNode*)new_next;
        new_node->length = remaining_size - sizeof(HeapNode);
        new_node->next = next;
        replacement = new_node;
    }

    if (prev){
        prev->next = replacement;
    } else {
        page->freelist = replacement;
    }

    // update max_size for heap page
    HeapNode *cursor = page->freelist;
    uint64_t max_size = 0;
    while(cursor){
        if (cursor->length > max_size){
            max_size = cursor->length;
        }
        cursor = cursor->next;
    }
    page->largest = max_size;

    return return_base;
}

bool kfree(uint64_t *address){
    uint64_t *virtual_frame = pointer_frame(address);
    uint64_t physical_frame = virtual_to_physical(virtual_frame);

    uint64_t *base = (uint64_t*)(pointer(address) - sizeof(uint64_t));
    HeapNode *node = (HeapNode*)(pointer(base) - sizeof(HeapNode));
    node->length = *base;

    HeapPage *page = (HeapPage*)virtual_frame;
    // walk the list and find the insert position
    HeapNode *cursor = (HeapNode*)page->freelist;
    HeapNode *prev;
    prev = NULL;
    while (cursor && ((uint64_t)cursor < (uint64_t)node)){
        prev = cursor;
        cursor = cursor->next;
    }

    HeapNode *merge_cursor;
    // insert at head
    if (!prev){
        node->next = cursor;
        page->freelist = node;

        merge_cursor = node;
    } else {
        // insert between prev and cursor
        prev->next = node;
        node->next = cursor;

        merge_cursor = prev;
    }

    while (merge_cursor && merge_cursor->next){
        uint8_t *end_of_cursor = pointer(merge_cursor) + sizeof(HeapNode) + merge_cursor->length + sizeof(uint64_t);
        if (end_of_cursor == (uint8_t*)(merge_cursor->next)){
            merge_cursor->length += sizeof(HeapNode);
            merge_cursor->length += merge_cursor->next->length;
            merge_cursor->next = merge_cursor->next->next;
        } else {
            merge_cursor = merge_cursor->next;
        }
    }

    return true;
}

bool heapnode_is_empty(HeapNode *node)
{
    if (node->length == SIZE_4KB - sizeof(HeapNode)){
        return true;
    }
    return false;
}
