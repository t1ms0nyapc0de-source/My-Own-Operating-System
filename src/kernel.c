#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gdt.h"
#include "idt.h"
#include "syscall.h"
#include "user_mode.h"
#include "pmm.h"
#include "vmm.h"
#include "multiboot.h"
#include "task.h"
#include "timer.h"
#include "vfs.h"
#include "tar.h"
#include "elf.h"

/* Hardware text mode color constants. */
enum vga_color {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | (uint16_t)color << 8;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

/* Initialize the VGA text buffer */
void terminal_initialize(void) {
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  // 0xB8000 is the physical memory address where the VGA text buffer lives
  terminal_buffer = (uint16_t *)0xB8000;
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
  }
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
  if (c == '\n') {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT) {
      terminal_row = 0;
    }
    return;
  }

  terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
  if (++terminal_column == VGA_WIDTH) {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT)
      terminal_row = 0;
  }
}

void terminal_writestring(const char *data) {
  size_t i = 0;
  while (data[i] != '\0') {
    terminal_putchar(data[i]);
    i++;
  }
}

void terminal_writehex(uint32_t n) {
  static const char hex[] = "0123456789ABCDEF";
  char buf[11];
  buf[0] = '0';
  buf[1] = 'x';
  for (int i = 9; i >= 2; i--) {
    buf[i] = hex[n & 0xF];
    n >>= 4;
  }
  buf[10] = '\0';
  terminal_writestring(buf);
}

void terminal_writedec(uint32_t n) {
  if (n == 0) {
    terminal_putchar('0');
    return;
  }
  char buf[32];
  int i = 0;
  while (n > 0) {
    buf[i++] = '0' + (n % 10);
    n /= 10;
  }
  for (int j = i - 1; j >= 0; j--) {
    terminal_putchar(buf[j]);
  }
}

void task_a(void);
void task_b(void);

void kernel_main(uint32_t magic, uint32_t mbi_addr) {
  /* Initialize terminal interface */
  terminal_initialize();

  terminal_writestring("[Kernel] Initializing core systems...\n");

  /* 1. Verify Multiboot signature */
  if (magic != 0x2BADB002) {
    terminal_writestring("[Kernel] Error: Invalid Multiboot magic: ");
    terminal_writehex(magic);
    terminal_writestring("\nSystem Halted.\n");
    for (;;) { asm volatile("hlt"); }
  }

  /* 2. Initialize GDT & TSS */
  gdt_init();
  terminal_writestring("[Kernel] GDT and TSS successfully initialized.\n");

  /* 3. Initialize IDT & ISR Exception gates */
  idt_init();
  terminal_writestring("[Kernel] IDT and ISR exception handlers registered.\n");

  /* 4. Initialize System Calls */
  syscall_init();
  terminal_writestring("[Kernel] System Call interface registered.\n");

  /* 5. Initialize Physical Memory Manager */
  pmm_init(mbi_addr);
  terminal_writestring("[Kernel] Physical Memory Manager (PMM) initialized.\n");

  /* Display PMM Discovery Details */
  uint32_t total_blocks = pmm_get_total_blocks();
  uint32_t free_blocks  = pmm_get_free_blocks();
  uint32_t used_blocks  = pmm_get_used_blocks();
  
  terminal_writestring("  -> Total RAM tracked: ");
  terminal_writedec((total_blocks * PMM_BLOCK_SIZE) / (1024 * 1024));
  terminal_writestring(" MB (");
  terminal_writedec(total_blocks);
  terminal_writestring(" blocks)\n");
  
  terminal_writestring("  -> Free blocks: ");
  terminal_writedec(free_blocks);
  terminal_writestring(" | Used/Reserved blocks: ");
  terminal_writedec(used_blocks);
  terminal_writestring("\n");

  /* 6. Run PMM Diagnostic Test Suite */
  terminal_writestring("[Kernel] Running PMM diagnostic tests...\n");

  /* Allocate three 4KB page frames */
  void *block1 = pmm_alloc_block();
  void *block2 = pmm_alloc_block();
  void *block3 = pmm_alloc_block();

  terminal_writestring("  -> Allocated Block 1: "); terminal_writehex((uint32_t)block1); terminal_writestring("\n");
  terminal_writestring("  -> Allocated Block 2: "); terminal_writehex((uint32_t)block2); terminal_writestring("\n");
  terminal_writestring("  -> Allocated Block 3: "); terminal_writehex((uint32_t)block3); terminal_writestring("\n");

  /* Assert non-NULL and alignment */
  if (!block1 || !block2 || !block3) {
    terminal_writestring("[Kernel] PMM TEST FAILED: Allocation returned NULL!\n");
    for (;;) { asm volatile("hlt"); }
  }
  if (((uint32_t)block1 % 4096 != 0) || ((uint32_t)block2 % 4096 != 0) || ((uint32_t)block3 % 4096 != 0)) {
    terminal_writestring("[Kernel] PMM TEST FAILED: Blocks are not 4KB aligned!\n");
    for (;;) { asm volatile("hlt"); }
  }

  /* Free Block 2 and verify reuse (First-Fit assertion) */
  terminal_writestring("  -> Freeing Block 2...\n");
  pmm_free_block(block2);

  void *block_reuse = pmm_alloc_block();
  terminal_writestring("  -> Allocated new block (expecting Block 2 reuse): ");
  terminal_writehex((uint32_t)block_reuse);
  terminal_writestring("\n");

  if (block_reuse != block2) {
    terminal_writestring("[Kernel] PMM TEST FAILED: First-fit reuse failed!\n");
    for (;;) { asm volatile("hlt"); }
  }

  /* Clean up all test allocations */
  pmm_free_block(block1);
  pmm_free_block(block_reuse);
  pmm_free_block(block3);

  uint32_t free_blocks_after = pmm_get_free_blocks();
  if (free_blocks_after != free_blocks) {
    terminal_writestring("[Kernel] PMM TEST FAILED: Memory leak detected! Free blocks did not restore.\n");
    for (;;) { asm volatile("hlt"); }
  }

  terminal_writestring("[Kernel] PMM diagnostic tests passed successfully!\n\n");

  /* 7. Initialize Virtual Memory Manager (VMM) */
  terminal_writestring("[Kernel] Initializing Virtual Memory Manager (VMM)...\n");
  vmm_init();

  /* 8. Run VMM Diagnostic Test Suite */
  terminal_writestring("[Kernel] Running VMM diagnostic tests...\n");

  /* Test A: Dynamic Mapping & Write/Read Verification */
  terminal_writestring("  -> Test A: Dynamic Mapping & Write Verification...\n");
  void *phys_test = pmm_alloc_block();
  if (!phys_test) {
    terminal_writestring("  [VMM TEST FAILED] PMM allocation returned NULL!\n");
    for (;;) { asm volatile("hlt"); }
  }

  /* Map 0xA0000000 to the physical block (Present | Write | User) */
  vmm_map_page(phys_test, (void *)0xA0000000, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
  
  /* Write a verification signature */
  volatile uint32_t *test_ptr = (volatile uint32_t *)0xA0000000;
  *test_ptr = 0xCAFEBABE;

  /* Read and verify signature */
  if (*test_ptr != 0xCAFEBABE) {
    terminal_writestring("  [VMM TEST FAILED] Read/write verification signature mismatch!\n");
    for (;;) { asm volatile("hlt"); }
  }
  terminal_writestring("  -> Test A Passed: Virtual page successfully mapped, written, and verified.\n");

  /* Clean up Test A */
  vmm_unmap_page((void *)0xA0000000);
  pmm_free_block(phys_test);

  /* Test B: Demand Paging Verification */
  terminal_writestring("  -> Test B: Demand Paging Verification...\n");
  terminal_writestring("     (Writing to unmapped 0xC0000000 to trigger demand allocation)\n");
  
  /* 
   * This write will trigger a Page Fault. The handler will intercept it,
   * map it dynamically, and return here to re-execute the write!
   */
  volatile uint32_t *demand_ptr = (volatile uint32_t *)0xC0000000;
  *demand_ptr = 0x12345678;

  /* Verify the written value */
  if (*demand_ptr != 0x12345678) {
    terminal_writestring("  [VMM TEST FAILED] Demand paging value mismatch!\n");
    for (;;) { asm volatile("hlt"); }
  }
  terminal_writestring("  -> Test B Passed: Demand paging dynamically resolved non-present access.\n");

  /* Clean up Test B */
  vmm_unmap_page((void *)0xC0000000);

  terminal_writestring("[Kernel] VMM diagnostic tests passed successfully!\n\n");

  /* 9. Prepare Supervisor Protection Security Test */
  /* Map 0xD0000000 as Supervisor-only (NO User flag: VMM_FLAG_PRESENT | VMM_FLAG_WRITE) */
  void *phys_sec = pmm_alloc_block();
  if (!phys_sec) {
    terminal_writestring("  [Kernel] Error: Failed to allocate physical frame for security test.\n");
    for (;;) { asm volatile("hlt"); }
  }
  vmm_map_page(phys_sec, (void *)0xD0000000, VMM_FLAG_PRESENT | VMM_FLAG_WRITE);

  /* 10. Initialize VFS and RAM Disk (TAR filesystem) */
  terminal_writestring("[Kernel] Initializing Virtual File System & RAM Disk...\n");
  uint32_t initrd_start = 0;
  uint32_t initrd_end = 0;
  multiboot_info_t *mbi = (multiboot_info_t *)mbi_addr;
  if (mbi->flags & 0x00000008) { // MULTIBOOT_INFO_MODS
      if (mbi->mods_count > 0) {
          struct multiboot_mod_list {
              uint32_t mod_start;
              uint32_t mod_end;
              uint32_t cmdline;
              uint32_t pad;
          } __attribute__((packed)) *mods = (struct multiboot_mod_list *)mbi->mods_addr;
          initrd_start = mods[0].mod_start;
          initrd_end = mods[0].mod_end;
      }
  }

  if (initrd_start != 0 && initrd_end > initrd_start) {
      tar_init(initrd_start, initrd_end - initrd_start);
  } else {
      terminal_writestring("[Kernel] Warning: No RAM disk module found in Multiboot info!\n");
  }

  /* 11. Initialize Scheduler */
  terminal_writestring("[Kernel] Initializing Scheduler...\n");
  scheduler_init();

  /* 12. Create Demo Tasks */
  terminal_writestring("[Kernel] Creating Tasks A and B...\n");
  task_create(task_a, 0, false);
  task_create(task_b, 0, false);

  /* 13. Load and Create User ELF Task */
  if (initrd_start != 0) {
      uint32_t elf_entry = 0;
      uint32_t *elf_pd = NULL;
      if (elf_load("/hello", &elf_entry, &elf_pd) == 0) {
          terminal_writestring("[Kernel] Spawning user ELF task '/hello'...\n");
          task_create((void (*)(void))elf_entry, (uint32_t)elf_pd, true);
      } else {
          terminal_writestring("[Kernel] Error: Failed to load user ELF task '/hello'\n");
      }
  }

  /* 14. Initialize PIT Timer (Preemptive Scheduling, 100Hz) */
  terminal_writestring("[Kernel] Enabling Timer Interrupts...\n");
  timer_init(100);

  /* Enable interrupts globally */
  asm volatile("sti");

  /* 15. Idle loop for the boot thread */
  terminal_writestring("[Kernel] Boot thread entering idle loop.\n\n");
  for (;;) {
    asm volatile("hlt");
  }
}

/*
 * Demo Tasks for Preemptive Multitasking
 */
void task_a(void) {
    for (int count = 0; count < 5; count++) {
        terminal_writestring("[Task A] Running loop ");
        terminal_writedec(count);
        terminal_writestring("\n");
        for (volatile int i = 0; i < 20000000; i++) {} // Busy delay to allow preemption
    }
    terminal_writestring("[Task A] Finished.\n");
}

void task_b(void) {
    for (int count = 0; count < 5; count++) {
        terminal_writestring("[Task B] Running loop ");
        terminal_writedec(count);
        terminal_writestring("\n");
        for (volatile int i = 0; i < 20000000; i++) {} // Busy delay to allow preemption
    }
    terminal_writestring("[Task B] Finished.\n");
}