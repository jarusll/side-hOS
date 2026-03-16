#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#include "serial.h"


#define SIZE_4KB 0x1000

// Paging flags, flags for all levels are combined
#define PAGING_NO_EXECUTE 63

// paging masks used to extract pml4, pdpt, pd, pt
// and physical frame offset from virtual addr
#define PAGING_OFFSET_MASK 0xFFF
#define PAGING_PT_MASK 0x1FF << 12
#define PAGING_PD_MASK 0x1FF << 21
#define PAGING_PDPT_MASK 0x1FF << 30
#define PAGING_PML4_MASK 0x1FF << 39

#define DEFAULT_COLOR_SCHEME ((ColorScheme){ .fg = 0xFFFFFFFF, .bg = 0xFF })
#define DEFAULT_CURSOR ((TTYPoint){.x = 0, .y = 0})

char* kstdin();
void kstdout(char* str);
void kshell();
static void halt(void);
uint64_t virtual_to_physical(uint64_t);
uint64_t physical_to_virtual(uint64_t);
uint64_t alloc_frame();
uint64_t memory_nth_segment(Segment seg, uint64_t n);


typedef struct {
    uint64_t base;
    uint64_t length;
} Segment;

typedef struct {
    Segment segments[256];
    int8_t cursor;
} FreeList;

uint8_t inb(uint16_t port) {
    uint8_t ret;
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

#endif
