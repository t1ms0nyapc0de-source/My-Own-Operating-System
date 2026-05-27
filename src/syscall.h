#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

/*
 * System Call Numbers
 *
 * When a user program executes `int 0x80`, it places a syscall number
 * in EAX. The kernel dispatcher reads EAX and routes to the right function.
 * This mirrors how Linux assigns numbers in its ABI.
 */
#define SYSCALL_WRITE  0   /* Write string to VGA terminal */
#define SYSCALL_EXIT   1   /* Terminate the current process */

void syscall_init(void);

#endif /* SYSCALL_H */
