#include "syscall.h"
#include "idt.h"
#include "task.h"
#include <stdint.h>

extern void terminal_putchar(char c);
extern void terminal_writestring(const char *str);

/* -----------------------------------------------------------------------
 * sys_write — Syscall 0
 *
 * Registers on entry (set by user program before INT 0x80):
 *   EAX = 0           (syscall number)
 *   EBX = file descriptor  (1 = stdout; we only support stdout for now)
 *   ECX = pointer to string buffer
 *   EDX = number of bytes to write
 * ----------------------------------------------------------------------- */
static void sys_write(struct registers *regs) {
    const char *buf = (const char *)regs->ecx;
    uint32_t    len = regs->edx;

    for (uint32_t i = 0; i < len; i++) {
        terminal_putchar(buf[i]);
    }

    /* Return value: number of bytes written (stored in EAX for the caller) */
    regs->eax = len;
}

/* -----------------------------------------------------------------------
 * sys_exit — Syscall 1
 *
 * Registers on entry:
 *   EAX = 1           (syscall number)
 *   EBX = exit status code
 *
 * Terminates the current "process" and halts the CPU.
 * In a full kernel this would schedule the next process instead.
 * ----------------------------------------------------------------------- */
static void sys_exit(struct registers *regs) {
    terminal_writestring("\n[Kernel] Process exited with status ");
    char c = '0' + (char)(regs->ebx % 10);
    terminal_putchar(c);
    terminal_writestring(".\n");

    /* Terminate the current task and run the next one */
    task_exit();
}

/* -----------------------------------------------------------------------
 * syscall_dispatch — Central dispatcher (registered on INT 0x80)
 *
 * Reads EAX and calls the matching syscall handler.
 * This is exactly how Linux's int 0x80 ABI works on 32-bit x86.
 * ----------------------------------------------------------------------- */
static void syscall_dispatch(struct registers *regs) {
    switch (regs->eax) {
        case SYSCALL_WRITE: sys_write(regs); break;
        case SYSCALL_EXIT:  sys_exit(regs);  break;
        default:
            terminal_writestring("[Kernel] Warning: unknown syscall\n");
            regs->eax = (uint32_t)-1; /* Return -1 to caller */
            break;
    }
}

/* Register the dispatcher on interrupt vector 128 (0x80) */
void syscall_init(void) {
    idt_register_handler(128, syscall_dispatch);
}
