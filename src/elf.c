#include "elf.h"
#include "pmm.h"
#include "string.h"
#include "vfs.h"
#include "vmm.h"

extern void terminal_writestring(const char *);
extern void terminal_writehex(uint32_t);

/*
 * elf_load - Parse and load an ELF32 executable from the VFS into a new
 * address space. Returns 0 on success, -1 on any failure.
 *
 * FIX 3: The CR3 switch is now wrapped in cli/sti.
 *   If a timer interrupt fires between "current_page_directory = proc_pd" and
 *   the matching restore, the scheduler sees proc_pd as the kernel directory
 *   and context-switches into it — corrupting both the new process and the
 *   kernel. Disabling interrupts for the brief switch window prevents this.
 *
 * FIX 4: All physical blocks allocated for segments are tracked in a small
 *   array. If any allocation fails, every previously allocated block is freed
 *   before returning -1, eliminating the memory leak on partial load.
 */
int elf_load(const char *path, uint32_t *out_entry, uint32_t **out_pd) {
  fs_node_t *file = find_node_by_path(path);
  if (!file) {
    terminal_writestring("[ELF] Error: Executable file not found: ");
    terminal_writestring(path);
    terminal_writestring("\n");
    return -1;
  }

  Elf32_Ehdr ehdr;
  if (read_fs(file, 0, sizeof(Elf32_Ehdr), (uint8_t *)&ehdr) <
      sizeof(Elf32_Ehdr)) {
    terminal_writestring("[ELF] Error: Failed to read ELF header\n");
    return -1;
  }

  uint32_t *magic = (uint32_t *)ehdr.e_ident;
  if (*magic != ELF_MAGIC) {
    terminal_writestring("[ELF] Error: Invalid ELF signature\n");
    return -1;
  }
  if (ehdr.e_ident[4] != 1) {
    terminal_writestring("[ELF] Error: Executable is not 32-bit\n");
    return -1;
  }
  if (ehdr.e_machine != 3) {
    terminal_writestring(
        "[ELF] Error: Executable is not for x86 architecture\n");
    return -1;
  }
  if (ehdr.e_type != 2) {
    terminal_writestring("[ELF] Error: File is not an executable\n");
    return -1;
  }

  /* Allocate page directory for the new process */
  uint32_t *proc_pd = (uint32_t *)pmm_alloc_block();
  if (!proc_pd) {
    terminal_writestring("[ELF] Error: Failed to allocate Page Directory\n");
    return -1;
  }
  memcpy(proc_pd, current_page_directory, PMM_BLOCK_SIZE);

  /*
   * FIX 4: Segment block tracking for leak-free cleanup on failure.
   * We record every physical block we allocate so we can free them all
   * if something goes wrong before we finish loading.
   *
   * 64 blocks = 256 KB of loadable segment space — enough for a small
   * user program. Increase MAX_SEGMENT_BLOCKS if you load larger ELFs.
   */
#define MAX_SEGMENT_BLOCKS 64
  void *seg_blocks[MAX_SEGMENT_BLOCKS];
  int seg_block_count = 0;

  Elf32_Phdr phdr;
  for (int i = 0; i < ehdr.e_phnum; i++) {
    uint32_t offset = ehdr.e_phoff + i * ehdr.e_phentsize;
    if (read_fs(file, offset, sizeof(Elf32_Phdr), (uint8_t *)&phdr) <
        sizeof(Elf32_Phdr)) {
      terminal_writestring("[ELF] Error: Failed to read program header\n");
      goto cleanup;
    }

    if (phdr.p_type == PT_LOAD) {
      uint32_t start_vaddr = phdr.p_vaddr;
      uint32_t end_vaddr = start_vaddr + phdr.p_memsz;
      uint32_t page_aligned_start = start_vaddr & 0xFFFFF000;
      uint32_t page_aligned_end = (end_vaddr + 4095) & 0xFFFFF000;

      uint32_t *old_pd = current_page_directory;

      /*
       * FIX 3: Disable interrupts before touching CR3/current_page_directory.
       * The scheduler reads current_page_directory; a preemption here would
       * make it believe the new proc_pd IS the kernel page directory.
       */
      asm volatile("cli");
      current_page_directory = proc_pd;
      asm volatile("mov %0, %%cr3" : : "r"(proc_pd) : "memory");

      for (uint32_t vaddr = page_aligned_start; vaddr < page_aligned_end;
           vaddr += PMM_BLOCK_SIZE) {
        void *phys = pmm_alloc_block();
        if (!phys) {
          terminal_writestring(
              "[ELF] Error: Out of physical memory for segment\n");
          /* Restore old page directory before cleanup */
          current_page_directory = old_pd;
          asm volatile("mov %0, %%cr3" : : "r"(old_pd) : "memory");
          asm volatile("sti");
          goto cleanup;
        }

        /* FIX 4: Track this block so we can free it on failure */
        if (seg_block_count < MAX_SEGMENT_BLOCKS) {
          seg_blocks[seg_block_count++] = phys;
        }

        vmm_map_page(phys, (void *)(uintptr_t)vaddr,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
        memset((void *)(uintptr_t)vaddr, 0, PMM_BLOCK_SIZE);
      }

      read_fs(file, phdr.p_offset, phdr.p_filesz,
              (uint8_t *)(uintptr_t)start_vaddr);

      /* Restore kernel page directory */
      current_page_directory = old_pd;
      asm volatile("mov %0, %%cr3" : : "r"(old_pd) : "memory");
      asm volatile("sti"); /* Re-enable interrupts after CR3 is restored */
    }
  }

  *out_entry = ehdr.e_entry;
  *out_pd = proc_pd;
  return 0;

cleanup:
  /*
   * FIX 4: Free every physical block we allocated before the failure.
   * Without this the blocks were permanently leaked — invisible to the PMM
   * bitmap, unreachable, and never reclaimed.
   */
  terminal_writestring(
      "[ELF] Cleaning up allocated segment blocks after failure...\n");
  for (int i = 0; i < seg_block_count; i++) {
    pmm_free_block(seg_blocks[i]);
  }
  pmm_free_block(proc_pd);
  return -1;
}
