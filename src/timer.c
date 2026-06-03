// timer.c — PIT Timer and PIC remapping implementation
#include "timer.h"
#include "ports.h"
#include "idt.h"
#include "task.h"

extern void terminal_writestring(const char *str);
extern void terminal_writedec(uint32_t n);

static uint32_t tick = 0;

void timer_callback(struct registers *regs) {
    (void)regs;
    tick++;

    // Print a periodic heartbeat tick message every 100 ticks (approx 1 second at 100Hz)
    if (tick % 100 == 0) {
        terminal_writestring("[Timer] Tick: ");
        terminal_writedec(tick);
        terminal_writestring("\n");
    }

    // Call the scheduler to perform preemptive context switching
    schedule();
}

void timer_init(uint32_t frequency) {
    // 1. Remap the 8259 PIC (Programmable Interrupt Controller)
    // Send ICW1 (Initialization Command Word 1) - start initialization
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    // Send ICW2 - map Master PIC to vector 32 (0x20) and Slave PIC to vector 40 (0x28)
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    // Send ICW3 - tell Master PIC that there is a slave PIC at IRQ 2 (0x04)
    outb(0x21, 0x04);
    // Tell Slave PIC its cascade identity (2)
    outb(0xA1, 0x02);

    // Send ICW4 - set 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Clear masks (enable all IRQs on PICs)
    outb(0x21, 0x00);
    outb(0xA1, 0x00);

    // 2. Configure the PIT (Programmable Interval Timer)
    // The PIT runs at 1193182 Hz.
    uint32_t divisor = 1193182 / frequency;

    // Send control byte: Channel 0, Lobyte/Hibyte, Mode 3 (square wave), binary
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    // 3. Register the timer interrupt handler at IDT vector 32 (IRQ 0)
    idt_register_handler(32, timer_callback);

    terminal_writestring("[Timer] Initialized PIT at ");
    terminal_writedec(frequency);
    terminal_writestring(" Hz.\n");
}
