#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#include "serial.h"


#define SIZE_4KB 0x1000

// Paging flags, flags for all levels are combined
#define PAGING_PRESENT_FLAG 0
#define PAGING_READ_WRITE_FLAG 1
#define PAGING_USER_SUPERVISOR_FLAG 2
#define PAGING_PAGE_LEVEL_WRITE_THROUGH_FLAG 3
#define PAGING_PAGE_LEVEL_CACHE_DISABLE_FLAG 4
#define PAGING_ACCESSED_FLAG 5
#define PAGING_DIRTY_FLAG 6
#define PAGING_PAGE_SIZE_FLAG 7
#define PAGING_GLOBAL_PAGE_FLAG 8
#define PAGING_AVAILABLE_FLAG 0xe00 // 11:9

#define PAGING_NO_EXECUTE_FLAG 63

// paging masks used to extract pml4, pdpt, pd, pt
// and physical frame offset from virtual addr
#define PAGING_OFFSET_MASK 0xFFF
#define PAGING_PT_MASK 0x1FF << 12
#define PAGING_PD_MASK 0x1FF << 21
#define PAGING_PDPT_MASK 0x1FF << 30
#define PAGING_PML4_MASK 0x1FF << 39

// shifts
#define PAGING_OFFSET_SHIFT 0
#define PAGING_PT_SHIFT 12
#define PAGING_PD_SHIFT 21
#define PAGING_PDPT_SHIFT 30
#define PAGING_PML4_SHIFT 39

#define PAGING_NEXT_BASE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL

#define DEFAULT_COLOR_SCHEME ((ColorScheme){ .fg = 0xFFFFFFFF, .bg = 0xFF })
#define DEFAULT_CURSOR ((TTYPoint){.x = 0, .y = 0})

typedef struct Segment {
    uint64_t base;
    uint64_t length;
} Segment;

typedef struct PointerList {
    uint64_t *entries[UINT8_MAX];
    uint8_t cursor;
} PointerList;

typedef struct FreeList {
    Segment segments[255];
    int8_t cursor;
} FreeList;

typedef struct HeapNode {
    int64_t length;
    struct HeapNode *next;
} HeapNode;

typedef struct HeapPage {
    HeapNode *freelist;
    uint64_t largest;
    struct HeapPage *next;
} HeapPage;

// because each physical frame contains HeapPage, HeapNode & u64
#define SMALL_PAGE_MAXIMUM_SIZE SIZE_4KB - sizeof(HeapPage) - sizeof(HeapNode) - sizeof(uint64_t)

typedef struct MemoryContext {
    FreeList freelist;
    HeapPage *heap_pages;
} MemoryContext;

char* kstdin();
void kstdout(char* str);
void puthex(uint64_t value);
void kshell();
static void halt(void);
uint64_t virtual_to_physical(uint64_t*);
uint64_t* physical_to_virtual(uint64_t);
uint64_t alloc_frame();
bool free_frame(uint64_t physical);
uint64_t memory_nth_segment(Segment *seg, uint64_t n);
uint64_t* kmalloc(uint64_t size);
bool kfree(uint64_t*);
bool heapnode_is_empty(HeapNode *node);


uint8_t inb(uint16_t port) { uint8_t ret;
    asm volatile ("inb %1, %0"
                    : "=a"(ret)
                    : "Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
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

static inline uint8_t* pointer(void *ptr){
    return (uint8_t*)ptr;
}

static inline void* pointer_add(void* base, size_t n){
    return (void*)((uint8_t*)base + n);
}

static inline void* pointer_sub(void* base, size_t n){
    return (void*)((uint8_t*)base - n);
}

static inline void* pointer_frame(void *address){
    return (void*)((uint64_t)address & ~(uint64_t)0xFFF);
}

#endif
