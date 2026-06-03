#include "task.h"
#include "pmm.h"
#include "string.h"

#define MAX_TASKS 32

static task_t task_table[MAX_TASKS];
static task_t *task_list = NULL;
static task_t *current_task = NULL;
static uint32_t next_pid = 1;

extern void switch_task(task_t *current, task_t *next);

static inline uint32_t read_cr3(void) {
    uint32_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

task_t *get_current_task(void) {
    return current_task;
}

void task_kernel_bootstrap(void (*entry)(void)) {
    // Enable interrupts on starting the new task
    asm volatile("sti");
    entry();
    task_exit();
}

void scheduler_init(void) {
    asm volatile("cli");

    // Initialize all tasks in the table
    memset(task_table, 0, sizeof(task_table));

    // Initialize the boot thread (PID 0)
    task_t *boot_task = &task_table[0];
    boot_task->pid = 0;
    boot_task->state = TASK_STATE_RUNNING;
    boot_task->page_directory = read_cr3();
    boot_task->kstack = 0; // Already running in kernel space
    boot_task->next = NULL;

    task_list = boot_task;
    current_task = boot_task;

    asm volatile("sti");
}

task_t *task_create(void (*entry)(void), uint32_t page_dir, bool is_user) {
    (void)is_user; // Unused for now, all tasks run as kernel threads initially
    
    asm volatile("cli");

    // Find free slot
    task_t *new_task = NULL;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state == TASK_STATE_ZOMBIE || task_table[i].pid == 0) {
            new_task = &task_table[i];
            break;
        }
    }

    if (!new_task) {
        asm volatile("sti");
        return NULL; // Task table full
    }

    // Allocate stack frame
    void *stack = pmm_alloc_block();
    if (!stack) {
        new_task->pid = 0; // Release slot
        asm volatile("sti");
        return NULL;
    }
    
    // Clear stack memory
    memset(stack, 0, PMM_BLOCK_SIZE);

    new_task->pid = next_pid++;
    new_task->state = TASK_STATE_READY;
    new_task->page_directory = page_dir ? page_dir : read_cr3();
    new_task->kstack = (uint32_t)stack + PMM_BLOCK_SIZE;

    // Set up the initial stack contents for switch_task to pop
    uint32_t *stack_ptr = (uint32_t *)new_task->kstack;
    stack_ptr[-1] = (uint32_t)entry;                     // Argument to task_kernel_bootstrap
    stack_ptr[-2] = 0;                                    // Dummy return address
    stack_ptr[-3] = (uint32_t)task_kernel_bootstrap;         // Address to jump to via ret
    stack_ptr[-4] = 0;                                    // EBP
    stack_ptr[-5] = 0;                                    // EBX
    stack_ptr[-6] = 0;                                    // ESI
    stack_ptr[-7] = 0;                                    // EDI

    new_task->esp = (uint32_t)&stack_ptr[-7];

    // Append to task list
    task_t *curr = task_list;
    if (!curr) {
        task_list = new_task;
    } else {
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_task;
    }
    new_task->next = NULL;

    asm volatile("sti");
    return new_task;
}

void schedule(void) {
    if (!current_task) return;

    // Find the next task in the queue
    task_t *curr = current_task->next;
    if (!curr) curr = task_list; // Wrap around to the head

    // Loop to find a READY task
    while (curr != current_task) {
        if (curr->state == TASK_STATE_READY) {
            break;
        }
        curr = curr->next;
        if (!curr) curr = task_list;
    }

    // Switch to the new task if one is found
    if (curr != current_task && curr->state == TASK_STATE_READY) {
        task_t *prev = current_task;
        if (prev->state == TASK_STATE_RUNNING) {
            prev->state = TASK_STATE_READY;
        }
        curr->state = TASK_STATE_RUNNING;
        current_task = curr;

        // Perform the low-level context switch
        switch_task(prev, curr);
    }
}

void task_yield(void) {
    asm volatile("cli");
    schedule();
    asm volatile("sti");
}

void task_exit(void) {
    asm volatile("cli");
    current_task->state = TASK_STATE_ZOMBIE;
    schedule();
    
    // We should never reach here
    for (;;) { asm volatile("hlt"); }
}
