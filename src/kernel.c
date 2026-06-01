#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gdt.h"
#include "idt.h"
#include "syscall.h"
#include "user_mode.h"
#include "pmm.h"
#include "multiboot.h"

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

  /* 7. Transition to Ring 3 User Mode */
  terminal_writestring("[Kernel] Transitioning to Ring 3 User Mode...\n\n");
  run_user_demo();

  /* Should never reach here */
  for (;;) {
    asm volatile("hlt");
  }
}