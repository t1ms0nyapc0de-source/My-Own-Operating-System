#include "user_mode.h"
#include "gdt.h"
#include "syscall.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Stacks
 *
 * We need two separate stacks:
 *
 *  1. kernel_stack — The stack the CPU switches to (via the TSS) when an
 *     interrupt fires while the CPU is running in Ring 3. This is the
 *     stack that our interrupt handlers (IDT + syscall) will use.
 *
 *  2. user_stack   — The stack the user program itself runs on in Ring 3.
 *
 * Both are statically allocated here. In a real kernel each *process*
 * would get its own dynamically-allocated stacks.
 * ----------------------------------------------------------------------- */
#define STACK_SIZE 4096

static uint8_t kernel_stack[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t user_stack  [STACK_SIZE] __attribute__((aligned(16)));

/* enter_user_mode is implemented in user_switch.s */
extern void enter_user_mode(uint32_t entry, uint32_t user_esp);

/* -----------------------------------------------------------------------
 * user_program — Runs entirely in Ring 3 (privilege level 3).
 *
 * It cannot call kernel functions directly. Any service it needs
 * (printing, exiting) must go through the INT 0x80 syscall gateway.
 *
 * Helper macros below issue the correct inline assembly.
 * ----------------------------------------------------------------------- */

/* Inline syscall helpers — mirror what a real user-space libc would do. */

static inline void syscall_write(const char *str, uint32_t len) {
    asm volatile (
        "int $0x80"
        :                                   /* no output operands */
        : "a"(0),                           /* EAX = SYSCALL_WRITE (0) */
          "b"(1),                           /* EBX = fd 1 (stdout) */
          "c"((uint32_t)str),               /* ECX = buffer address */
          "d"(len)                          /* EDX = byte count */
        : "memory"
    );
}

static inline void syscall_exit(uint32_t status) {
    asm volatile (
        "int $0x80"
        :
        : "a"(1),           /* EAX = SYSCALL_EXIT (1) */
          "b"(status)       /* EBX = exit status code */
        : "memory"
    );
}

/* strlen for the user program (no libc available) */
static uint32_t user_strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static void user_program(void) {
    /*
     * Every line below that produces output goes through INT 0x80.
     * The CPU traps to the kernel, runs sys_write, then returns here.
     * This demonstrates the full Ring 3 → Ring 0 → Ring 3 round-trip.
     */
    const char *msg1 = "[User] Hello from Ring 3 (User Mode)!\n";
    const char *msg2 = "[User] Calling sys_write via INT 0x80...\n";
    const char *msg3 = "[User] Attempting to write to Supervisor-protected page at 0xD0000000...\n";

    syscall_write(msg1, user_strlen(msg1));
    syscall_write(msg2, user_strlen(msg2));
    syscall_write(msg3, user_strlen(msg3));

    /* 
     * This write should trigger an immediate Page Fault exception
     * since 0xD0000000 is mapped as Supervisor-only (flags = 0x3)
     */
    volatile uint32_t *ptr = (volatile uint32_t *)0xD0000000;
    *ptr = 0xBAD1DEA;

    /* Should never reach here due to the page fault panic */
    syscall_exit(0);

    /* Should never reach here — sys_exit halts the CPU */
    for (;;) {}
}

/* -----------------------------------------------------------------------
 * run_user_demo
 *
 * 1. Tell the TSS where the kernel stack lives. The CPU reads this every
 *    time it transitions from Ring 3 to Ring 0 so it has a safe kernel
 *    stack to use for the interrupt handler.
 * 2. Call enter_user_mode (in user_switch.s) which constructs an iret
 *    frame and drops to Ring 3, jumping to user_program.
 * ----------------------------------------------------------------------- */
#include "task.h"

void run_user_demo(void) {
    /* Stack grows downward: pass the *top* of the stack array */
    uint32_t kstack_top = get_current_task() ? get_current_task()->kstack : ((uint32_t)kernel_stack + STACK_SIZE);
    uint32_t ustack_top = (uint32_t)user_stack   + STACK_SIZE;

    tss_set_kernel_stack(kstack_top);
    enter_user_mode((uint32_t)user_program, ustack_top);
}
