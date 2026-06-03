#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>

#define TASK_STATE_READY     0
#define TASK_STATE_RUNNING   1
#define TASK_STATE_BLOCKED   2
#define TASK_STATE_ZOMBIE    3

typedef struct task {
    uint32_t pid;             // offset 0
    uint32_t esp;             // offset 4: saved stack pointer
    uint32_t eip;             // offset 8: instruction pointer
    uint32_t page_directory;  // offset 12: CR3 value
    uint32_t kstack;          // offset 16: top of kernel stack
    uint32_t state;           // offset 20: READY, RUNNING, ZOMBIE
    struct task *next;        // offset 24: next task in queue
} task_t;

void scheduler_init(void);
task_t *task_create(void (*entry)(void), uint32_t page_dir, bool is_user);
void schedule(void);
void task_exit(void);
void task_yield(void);

task_t *get_current_task(void);

#endif /* TASK_H */
