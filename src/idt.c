#include "idt.h"
#include <string.h>

extern void idt_flush(uint32_t idt_ptr_addr);

/* Forward-declare every ISR stub defined in isr.s */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void isr128(void); /* int 0x80 — system call entry */

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

/* Table of registered C-level handler functions (NULL = unhandled) */
static isr_handler_t handlers[256];

/*
 * idt_set_gate - Encode one IDT descriptor.
 *
 * @vector  : Interrupt vector number (0–255).
 * @handler : Address of the assembly ISR stub.
 * @sel     : GDT selector the CPU switches to (0x08 = Kernel Code).
 * @flags   : Type/attribute byte (see idt.h for bit layout).
 */
static void idt_set_gate(uint8_t vector, void (*handler)(void),
                          uint16_t sel, uint8_t flags) {
    uint32_t addr = (uint32_t)handler;
    idt[vector].base_low  = addr & 0xFFFF;
    idt[vector].base_high = (addr >> 16) & 0xFFFF;
    idt[vector].selector  = sel;
    idt[vector].zero      = 0;
    idt[vector].flags     = flags;
}

/*
 * idt_init - Populate and load the IDT.
 *
 * Flags used:
 *   0x8E = 1000 1110b — Present, Ring 0, 32-bit interrupt gate
 *   0xEE = 1110 1110b — Present, Ring 3, 32-bit interrupt gate
 *          ^^^^ DPL=3 lets user-space code call INT 0x80 without a GPF
 */
void idt_init(void) {
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint32_t)&idt;

    memset(&idt, 0, sizeof(idt));
    memset(handlers, 0, sizeof(handlers));

    /* CPU exceptions 0–31 (Ring 0 only — user cannot trigger these directly) */
    idt_set_gate(0,  isr0,  0x08, 0x8E); /* Divide-by-Zero           */
    idt_set_gate(1,  isr1,  0x08, 0x8E); /* Debug                    */
    idt_set_gate(2,  isr2,  0x08, 0x8E); /* Non-Maskable Interrupt    */
    idt_set_gate(3,  isr3,  0x08, 0x8E); /* Breakpoint               */
    idt_set_gate(4,  isr4,  0x08, 0x8E); /* Overflow                  */
    idt_set_gate(5,  isr5,  0x08, 0x8E); /* Bound Range Exceeded      */
    idt_set_gate(6,  isr6,  0x08, 0x8E); /* Invalid Opcode            */
    idt_set_gate(7,  isr7,  0x08, 0x8E); /* Device Not Available      */
    idt_set_gate(8,  isr8,  0x08, 0x8E); /* Double Fault              */
    idt_set_gate(9,  isr9,  0x08, 0x8E); /* Coprocessor Segment Overrun */
    idt_set_gate(10, isr10, 0x08, 0x8E); /* Invalid TSS               */
    idt_set_gate(11, isr11, 0x08, 0x8E); /* Segment Not Present       */
    idt_set_gate(12, isr12, 0x08, 0x8E); /* Stack-Segment Fault       */
    idt_set_gate(13, isr13, 0x08, 0x8E); /* General Protection Fault  */
    idt_set_gate(14, isr14, 0x08, 0x8E); /* Page Fault                */
    idt_set_gate(15, isr15, 0x08, 0x8E); /* Reserved                  */
    idt_set_gate(16, isr16, 0x08, 0x8E); /* x87 FPU Error             */
    idt_set_gate(17, isr17, 0x08, 0x8E); /* Alignment Check           */
    idt_set_gate(18, isr18, 0x08, 0x8E); /* Machine Check             */
    idt_set_gate(19, isr19, 0x08, 0x8E); /* SIMD FP Exception         */
    idt_set_gate(20, isr20, 0x08, 0x8E); /* Virtualization Exception  */
    idt_set_gate(21, isr21, 0x08, 0x8E);
    idt_set_gate(22, isr22, 0x08, 0x8E);
    idt_set_gate(23, isr23, 0x08, 0x8E);
    idt_set_gate(24, isr24, 0x08, 0x8E);
    idt_set_gate(25, isr25, 0x08, 0x8E);
    idt_set_gate(26, isr26, 0x08, 0x8E);
    idt_set_gate(27, isr27, 0x08, 0x8E);
    idt_set_gate(28, isr28, 0x08, 0x8E);
    idt_set_gate(29, isr29, 0x08, 0x8E);
    idt_set_gate(30, isr30, 0x08, 0x8E);
    idt_set_gate(31, isr31, 0x08, 0x8E);

    /*
     * INT 0x80 — System Call Gate
     * DPL = 3 (0xEE) is critical: it allows code running in Ring 3 to
     * trigger this interrupt without causing a General Protection Fault.
     */
    idt_set_gate(128, isr128, 0x08, 0xEE);

    idt_flush((uint32_t)&idtp);
}

/*
 * idt_register_handler - Attach a C function to an interrupt vector.
 * Called by subsystems (e.g. syscall, timer) to handle specific vectors.
 */
void idt_register_handler(uint8_t vector, isr_handler_t handler) {
    handlers[vector] = handler;
}

/*
 * isr_dispatch - Common C dispatcher called by every assembly ISR stub.
 * Looks up the registered handler and calls it, or panics if unhandled.
 */
void isr_dispatch(struct registers *regs) {
    if (handlers[regs->int_no]) {
        handlers[regs->int_no](regs);
        return;
    }

    /* Unhandled exception — print details and halt */
    extern void terminal_writestring(const char *);
    terminal_writestring("\n\n*** KERNEL PANIC: Unhandled Exception ***\n");
    terminal_writestring("Exception #");

    /* Print the vector number (0-255) */
    char buf[4];
    uint32_t n = regs->int_no;
    buf[0] = (n >= 100) ? ('0' + n / 100)       : ' ';
    buf[1] = (n >=  10) ? ('0' + (n % 100) / 10) : ' ';
    buf[2] = '0' + (n % 10);
    buf[3] = '\0';
    terminal_writestring(buf);
    terminal_writestring("  Error Code: ");
    /* Print error code as hex */
    static const char hex[] = "0123456789ABCDEF";
    char hbuf[11] = "0x00000000";
    uint32_t ec = regs->err_code;
    for (int i = 9; i >= 2; i--) { hbuf[i] = hex[ec & 0xF]; ec >>= 4; }
    terminal_writestring(hbuf);
    terminal_writestring("\nSystem Halted.\n");
    for (;;) { asm volatile("hlt"); }
}
