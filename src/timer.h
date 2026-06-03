#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "idt.h"

void timer_init(uint32_t frequency);
void timer_callback(struct registers *regs);

#endif /* TIMER_H */
