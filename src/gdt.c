#include "gdt.h"
#include <string.h>

/* Declare the external assembly flush routines */
extern void gdt_flush(uint32_t gdt_ptr_addr);
extern void tss_flush(void);

/* The GDT has 6 entries:
 *   0: Null descriptor      (required by x86 spec)
 *   1: Kernel Code (Ring 0)
 *   2: Kernel Data (Ring 0)
 *   3: User   Code (Ring 3)
 *   4: User   Data (Ring 3)
 *   5: TSS descriptor
 */
static struct gdt_entry gdt[6];
static struct gdt_ptr   gdtp;
static struct tss_entry tss;
static uint8_t tss_stack[4096];

/*
 * gdt_set_entry - Encode and write one GDT descriptor.
 *
 * @index    : Index into gdt[].
 * @base     : Linear start address of the segment.
 * @limit    : Segment limit (number of units, not bytes if G bit set).
 * @access   : Access byte  (present | DPL | type bits).
 * @gran     : Granularity byte (G | D/B | L | AVL | limit[19:16]).
 */
static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[index].base_low  =  base        & 0xFFFF;
    gdt[index].base_mid  = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;

    gdt[index].limit_low   =  limit        & 0xFFFF;
    gdt[index].granularity = (limit >> 16) & 0x0F;   /* upper 4 bits of limit */
    gdt[index].granularity |= gran & 0xF0;            /* upper nibble: flags   */

    gdt[index].access = access;
}

/*
 * tss_write - Populate the TSS and install its descriptor in the GDT.
 *
 * The TSS descriptor is a "system" descriptor (access byte 0x89).
 * We only care about ss0/esp0 — the kernel stack the CPU uses when
 * returning from Ring 3 via an interrupt or system call.
 */
static void tss_write(int index, uint16_t ss0, uint32_t esp0) {
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = (uint32_t)(sizeof(tss) - 1);

    /* Install TSS descriptor as a 32-bit available TSS (access = 0x89) */
    gdt_set_entry(index, base, limit, 0x89, 0x00);

    memset(&tss, 0, sizeof(tss));
    tss.ss0        = ss0;
    tss.esp0       = esp0;
    tss.iomap_base = (uint16_t)sizeof(tss); /* deny all direct port I/O from user space */
}

/*
 * gdt_init - Build and load the GDT, then load the TSS.
 *
 * Access byte breakdown (for code/data):
 *   Bit 7 : Present       (1 = valid descriptor)
 *   Bits 6-5 : DPL       (00 = Ring 0, 11 = Ring 3)
 *   Bit 4 : Descriptor type (1 = code/data, 0 = system)
 *   Bit 3 : Executable   (1 = code segment)
 *   Bit 2 : Direction/Conforming
 *   Bit 1 : Read/Write   (1 = allow reads on code / writes on data)
 *   Bit 0 : Accessed     (CPU sets this; start at 0)
 *
 * Granularity byte upper nibble:
 *   Bit 7 (G)  : 1 = limit in 4 KB pages
 *   Bit 6 (D/B): 1 = 32-bit protected mode
 *   Bits 5-4   : reserved / unused
 */
void gdt_init(void) {
    gdtp.limit = (uint16_t)(sizeof(gdt) - 1);
    gdtp.base  = (uint32_t)&gdt;

    gdt_set_entry(0, 0, 0,          0x00, 0x00); /* Null descriptor */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* Kernel Code: P=1 DPL=0 E=1 RW=1 */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* Kernel Data: P=1 DPL=0 E=0 RW=1 */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* User   Code: P=1 DPL=3 E=1 RW=1 */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* User   Data: P=1 DPL=3 E=0 RW=1 */

    /* Use a static 4KB boot stack for the TSS until tasks take over */
    tss_write(5, 0x10, (uint32_t)(tss_stack + sizeof(tss_stack)));
    gdt_flush((uint32_t)&gdtp); /* lgdt + far jump to reload CS */
    tss_flush();                /* ltr  to load Task Register   */
}

/*
 * tss_set_kernel_stack - Update the TSS with the current kernel stack top.
 * Called every time we are about to enter user mode so the CPU knows
 * where to restore the kernel stack on the next system call or interrupt.
 */
void tss_set_kernel_stack(uint32_t stack_top) {
    tss.esp0 = stack_top;
}
