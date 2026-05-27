#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/*
 * GDT Entry (Segment Descriptor)
 * Each entry describes a memory segment and its access privileges.
 * Total size: 8 bytes.
 */
struct gdt_entry {
    uint16_t limit_low;   /* Bits 0-15 of segment limit */
    uint16_t base_low;    /* Bits 0-15 of segment base address */
    uint8_t  base_mid;    /* Bits 16-23 of segment base address */
    uint8_t  access;      /* Access flags: present, ring level, type */
    uint8_t  granularity; /* Bits 16-19 of limit + flags (size, granularity) */
    uint8_t  base_high;   /* Bits 24-31 of segment base address */
} __attribute__((packed));

/*
 * GDT Pointer
 * Passed to the lgdt instruction to load the GDT.
 */
struct gdt_ptr {
    uint16_t limit; /* Size of the GDT in bytes, minus 1 */
    uint32_t base;  /* Linear address of the GDT */
} __attribute__((packed));

/*
 * Task State Segment (TSS)
 * The CPU reads this when switching from Ring 3 back to Ring 0
 * (e.g., during a system call or interrupt). Only ss0 and esp0 are
 * critical for our purposes — they define the kernel stack the CPU
 * will switch to.
 */
struct tss_entry {
    uint32_t prev_tss;  /* Link to previous TSS (unused) */
    uint32_t esp0;      /* Kernel stack pointer — CPU loads this on Ring 3 -> Ring 0 switch */
    uint32_t ss0;       /* Kernel stack segment — must be 0x10 (kernel data) */
    uint32_t esp1;      /* Ring 1 stack (unused) */
    uint32_t ss1;
    uint32_t esp2;      /* Ring 2 stack (unused) */
    uint32_t ss2;
    uint32_t cr3;       /* Page directory (unused at this stage) */
    uint32_t eip;       /* Saved EIP (CPU fills this automatically) */
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base; /* Offset to I/O permission bitmap; set to sizeof(tss_entry) to deny all */
} __attribute__((packed));

/* GDT segment selector offsets */
#define GDT_KERNEL_CODE  0x08   /* Ring 0 Code */
#define GDT_KERNEL_DATA  0x10   /* Ring 0 Data */
#define GDT_USER_CODE    0x18   /* Ring 3 Code (use with | 3 -> 0x1B) */
#define GDT_USER_DATA    0x20   /* Ring 3 Data (use with | 3 -> 0x23) */
#define GDT_TSS          0x28   /* TSS descriptor (use with | 3 -> 0x2B) */

void gdt_init(void);
void tss_set_kernel_stack(uint32_t stack_top);

#endif /* GDT_H */
