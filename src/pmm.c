#include "pmm.h"
#include "multiboot.h"
#include "string.h"

/* Linker symbols marking the boundaries of the kernel binary in memory */
extern uint8_t _kernel_start;
extern uint8_t _kernel_end;

/* Allocator state variables */
static uint32_t *pmm_bitmap = NULL;
static uint32_t pmm_ram_size = 0;
static uint32_t pmm_total_blocks = 0;
static uint32_t pmm_used_blocks = 0;

/*
 * Bit manipulation helpers
 * Each bit in pmm_bitmap tracks a 4KB block: 0 = free, 1 = reserved/allocated
 */
static inline void bitmap_set(uint32_t bit) {
  pmm_bitmap[bit / 32] |= (1U << (bit % 32));
}

static inline void bitmap_clear(uint32_t bit) {
  pmm_bitmap[bit / 32] &= ~(1U << (bit % 32));
}

static inline int bitmap_test(uint32_t bit) {
  return (pmm_bitmap[bit / 32] & (1U << (bit % 32))) != 0;
}

/*
 * pmm_reserve_region - Mark physical memory range as reserved (used)
 */
void pmm_reserve_region(uint32_t base, uint32_t size) {
  uint32_t start_block = base / PMM_BLOCK_SIZE;
  uint32_t block_count = (size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;

  for (uint32_t i = 0; i < block_count; i++) {
    uint32_t block = start_block + i;
    if (block < pmm_total_blocks) {
      if (!bitmap_test(block)) {
        bitmap_set(block);
        pmm_used_blocks++;
      }
    }
  }
}

/*
 * pmm_free_region - Mark physical memory range as free (usable)
 */
void pmm_free_region(uint32_t base, uint32_t size) {
  uint32_t start_block = base / PMM_BLOCK_SIZE;
  uint32_t block_count = size / PMM_BLOCK_SIZE;

  for (uint32_t i = 0; i < block_count; i++) {
    uint32_t block = start_block + i;
    if (block < pmm_total_blocks) {
      if (bitmap_test(block)) {
        bitmap_clear(block);
        pmm_used_blocks--;
      }
    }
  }
}

/*
 * pmm_init - Initialize tracking bitmap by parsing Multiboot memory map
 */
void pmm_init(uint32_t mbi_addr) {
  if (!mbi_addr) {
    return;
  }

  multiboot_info_t *mbi = (multiboot_info_t *)mbi_addr;
  if (!(mbi->flags & MULTIBOOT_INFO_MEM_MAP)) {
    return;
  }

  /* Step 1: Detect the maximum physical RAM address */
  multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
  uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;
  uint64_t max_addr = 0;

  while ((uint32_t)mmap < mmap_end) {
    if (mmap->type == 1) { /* Usable RAM */
      uint64_t entry_end = mmap->addr + mmap->len;
      if (entry_end > max_addr) {
        max_addr = entry_end;
      }
    }
    mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size +
                                      sizeof(mmap->size));
  }

  /* Cap RAM detection to 32-bit physical limit (4 GB) */
  if (max_addr > 0xFFFFFFFFULL) {
    max_addr = 0xFFFFFFFFULL;
  }

  pmm_ram_size = (uint32_t)max_addr;
  pmm_total_blocks = pmm_ram_size / PMM_BLOCK_SIZE;

  /*
   * FIX 1: Bitmap size must cover ALL blocks including the last partial word.
   * Old code: bitmap_size = (pmm_total_blocks / 32) * 4  — drops remainder
   * bits. New code: round up to the next full uint32_t, so every block gets a
   * bit.
   */
  uint32_t bitmap_size = ((pmm_total_blocks + 31) / 32) * 4;

  pmm_bitmap = (uint32_t *)&_kernel_end;

  /* Initially reserve all blocks by setting them to 1 */
  memset(pmm_bitmap, 0xFF, bitmap_size);
  pmm_used_blocks = pmm_total_blocks;

  /* Step 3: Populate free regions based on Multiboot memory map */
  mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
  while ((uint32_t)mmap < mmap_end) {
    if (mmap->type == 1) {
      pmm_free_region((uint32_t)mmap->addr, (uint32_t)mmap->len);
    }
    mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size +
                                      sizeof(mmap->size));
  }

  /* Step 4: Re-reserve memory zones that must NEVER be allocated */
  pmm_reserve_region(0, 0x100000);

  uint32_t kernel_start = (uint32_t)&_kernel_start;
  uint32_t bitmap_end = (uint32_t)pmm_bitmap + bitmap_size;
  pmm_reserve_region(kernel_start, bitmap_end - kernel_start);

  if (mbi->flags & 0x00000008) {
    struct multiboot_mod_list {
      uint32_t mod_start;
      uint32_t mod_end;
      uint32_t cmdline;
      uint32_t pad;
    } __attribute__((packed)) *mods =
        (struct multiboot_mod_list *)mbi->mods_addr;

    for (uint32_t i = 0; i < mbi->mods_count; i++) {
      uint32_t size = mods[i].mod_end - mods[i].mod_start;
      pmm_reserve_region(mods[i].mod_start, size);
    }
  }
}

/*
 * pmm_alloc_block - Find the first free block in the bitmap, reserve it,
 * and return its physical address.
 *
 * FIX 1 (continued): Use total_words rounded UP so the last partial word
 * is also scanned. Without this, blocks in the final word were permanently
 * invisible to the allocator even when free.
 */
void *pmm_alloc_block(void) {
  /* Round up: scan every word that holds at least one valid block bit */
  uint32_t total_words = (pmm_total_blocks + 31) / 32;

  for (uint32_t i = 0; i < total_words; i++) {
    if (pmm_bitmap[i] != 0xFFFFFFFFU) {
      for (int j = 0; j < 32; j++) {
        uint32_t bit = i * 32 + j;
        /* Guard: don't return a block beyond the tracked range */
        if (bit >= pmm_total_blocks)
          return NULL;
        if (!bitmap_test(bit)) {
          bitmap_set(bit);
          pmm_used_blocks++;
          return (void *)(bit * PMM_BLOCK_SIZE);
        }
      }
    }
  }
  return NULL; /* Out of memory */
}

/*
 * pmm_free_block - Unreserve a page frame, marking it free (0) in the bitmap
 */
void pmm_free_block(void *ptr) {
  uint32_t block = (uint32_t)ptr / PMM_BLOCK_SIZE;
  if (block < pmm_total_blocks) {
    if (bitmap_test(block)) {
      bitmap_clear(block);
      pmm_used_blocks--;
    }
  }
}

/*
 * Diagnostic Helper APIs
 */
uint32_t pmm_get_total_blocks(void) { return pmm_total_blocks; }

uint32_t pmm_get_free_blocks(void) {
  return pmm_total_blocks - pmm_used_blocks;
}

uint32_t pmm_get_used_blocks(void) { return pmm_used_blocks; }
