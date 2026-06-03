#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/*
 * IDT Entry (Interrupt Descriptor / Gate)
 * Each entry describes one interrupt or exception handler.
 * Total size: 8 bytes.
 */
struct idt_entry {
    uint16_t base_low;  /* Bits 0-15 of handler address */
    uint16_t selector;  /* GDT selector to use when the handler runs (0x08 = Kernel Code) */
    uint8_t  zero;      /* Reserved — always 0 */
    uint8_t  flags;     /* Type & attributes:
                         *   Bit 7   : Present (must be 1)
                         *   Bits 6-5: DPL — privilege level allowed to trigger this via INT
                         *             0 = only kernel, 3 = user space can trigger it
                         *   Bit 4   : Storage segment (0 for interrupt gates)
                         *   Bits 3-0: Gate type:
                         *             0xE = 32-bit interrupt gate (disables IRQs on entry)
                         *             0xF = 32-bit trap gate (does NOT disable IRQs)
                         */
    uint16_t base_high; /* Bits 16-31 of handler address */
} __attribute__((packed));

/*
 * IDT Pointer
 * Passed to the lidt instruction.
 */
struct idt_ptr {
    uint16_t limit; /* Size of the IDT in bytes, minus 1 */
    uint32_t base;  /* Linear address of the IDT */
} __attribute__((packed));

/*
 * CPU Register snapshot pushed by our assembly ISR stubs.
 * The C interrupt handler receives a pointer to this on the stack.
 *
 * Layout (low → high address, i.e., pushed last → first):
 *   ds                  ← pushed by us (data segment before switch)
 *   edi..eax            ← pushed by PUSHA
 *   int_no, err_code    ← pushed by us / by CPU
 *   eip, cs, eflags     ← pushed by CPU automatically
 *   (useresp, ss)       ← pushed by CPU only on privilege-level change
 */
struct registers {
    uint32_t ds;                                    /* Saved data segment */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* PUSHA dump */
    uint32_t int_no, err_code;                      /* Interrupt number + error code */
    uint32_t eip, cs, eflags, useresp, ss;          /* CPU-pushed frame */
} __attribute__((packed));

typedef void (*isr_handler_t)(struct registers *);

void idt_init(void);
void idt_register_handler(uint8_t vector, isr_handler_t handler);
void irq_dispatch(struct registers *regs);

/* External assembly IRQ stubs */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

#endif /* IDT_H */
