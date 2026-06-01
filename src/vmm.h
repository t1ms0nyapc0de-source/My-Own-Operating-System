#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include "idt.h"

/* 
 * Page Directory and Page Table Entry Flags (32-bit x86)
 */
#define VMM_FLAG_PRESENT  0x001  /* Page is present in memory */
#define VMM_FLAG_WRITE    0x002  /* Page is read/write (if 0, read-only) */
#define VMM_FLAG_USER     0x004  /* Page is accessible by user mode (Ring 3) */
#define VMM_FLAG_ACCESSED 0x020  /* Set by CPU when page is read or written */
#define VMM_FLAG_DIRTY    0x040  /* Set by CPU when page is written to (PTE only) */

/*
 * vmm_init - Initialize the Virtual Memory Manager.
 * Installs the Page Fault handler, creates a Page Directory,
 * identity maps all available RAM (with user-accessible permissions),
 * and enables paging in the CR0 and CR3 registers.
 */
void vmm_init(void);

/*
 * vmm_map_page - Maps a virtual address to a physical address.
 * If the corresponding page table does not exist, it is dynamically allocated.
 *
 * @phys_addr : Base physical address (must be 4KB aligned).
 * @virt_addr : Base virtual address (must be 4KB aligned).
 * @flags     : Combination of VMM_FLAG_PRESENT, VMM_FLAG_WRITE, VMM_FLAG_USER.
 */
void vmm_map_page(void *phys_addr, void *virt_addr, uint32_t flags);

/*
 * vmm_unmap_page - Clears a virtual address mapping, marking it non-present,
 * and flushes the TLB for that address.
 *
 * @virt_addr : Virtual address to unmap (must be 4KB aligned).
 */
void vmm_unmap_page(void *virt_addr);

/*
 * page_fault_handler - IDT 14 Exception handler.
 * Decodes the error code, logs diagnostic details (CR2, instruction, mode, etc.),
 * and performs demand paging if in the test range, otherwise triggers a kernel panic.
 */
void page_fault_handler(struct registers *regs);

#endif /* VMM_H */
