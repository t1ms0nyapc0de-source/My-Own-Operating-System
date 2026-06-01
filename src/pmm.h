#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

/* Page frame size (4 KB) */
#define PMM_BLOCK_SIZE 4096

/*
 * pmm_init - Initialize the physical memory manager using the Multiboot info.
 * Maps out free vs. reserved zones and sets up the tracking bitmap.
 *
 * @mbi_addr : Physical address of the Multiboot information structure.
 */
void pmm_init(uint32_t mbi_addr);

/*
 * pmm_alloc_block - Allocates one 4KB physical memory page frame.
 *
 * @return : Physical base address of the allocated block, or NULL if out of memory.
 */
void *pmm_alloc_block(void);

/*
 * pmm_free_block - Deallocates a 4KB physical page frame, making it reusable.
 *
 * @ptr : Physical base address of the block to free.
 */
void pmm_free_block(void *ptr);

/*
 * pmm_reserve_region - Manually mark a range of physical memory as allocated/reserved.
 * Used to protect system/kernel structures.
 *
 * @base : Base physical address.
 * @size : Size in bytes.
 */
void pmm_reserve_region(uint32_t base, uint32_t size);

/*
 * pmm_free_region - Manually mark a range of physical memory as free.
 *
 * @base : Base physical address.
 * @size : Size in bytes.
 */
void pmm_free_region(uint32_t base, uint32_t size);

/*
 * Diagnostic Helper APIs
 */
uint32_t pmm_get_total_blocks(void);
uint32_t pmm_get_free_blocks(void);
uint32_t pmm_get_used_blocks(void);

#endif /* PMM_H */
