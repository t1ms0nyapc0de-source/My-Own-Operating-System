#ifndef USER_MODE_H
#define USER_MODE_H

#include <stdint.h>

/*
 * enter_user_mode - Drop CPU privilege from Ring 0 to Ring 3 and jump
 *                   to a user-space function.
 *
 * @entry     : Virtual address of the function to run in Ring 3.
 * @user_esp  : Top of the stack the user program will use.
 *
 * This function does NOT return. The only way back to kernel mode is via
 * an interrupt (e.g., a system call on INT 0x80) or a CPU exception.
 *
 * The kernel stack used during those ring transitions is configured
 * in the TSS before this is called (see gdt.h: tss_set_kernel_stack).
 */
void enter_user_mode(uint32_t entry, uint32_t user_esp);

/*
 * run_user_demo - Allocate stacks, set up TSS, then switch to Ring 3.
 * Called from kernel_main once all subsystems are initialised.
 */
void run_user_demo(void);

#endif /* USER_MODE_H */
