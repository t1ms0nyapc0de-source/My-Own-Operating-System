#include "elf.h"
#include "vfs.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"

extern void terminal_writestring(const char *);
extern void terminal_writehex(uint32_t);

int elf_load(const char *path, uint32_t *out_entry, uint32_t **out_pd) {
    fs_node_t *file = find_node_by_path(path);
    if (!file) {
        terminal_writestring("[ELF] Error: Executable file not found: ");
        terminal_writestring(path);
        terminal_writestring("\n");
        return -1;
    }

    Elf32_Ehdr ehdr;
    if (read_fs(file, 0, sizeof(Elf32_Ehdr), (uint8_t *)&ehdr) < sizeof(Elf32_Ehdr)) {
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
        terminal_writestring("[ELF] Error: Executable is not for x86 architecture\n");
        return -1;
    }
    if (ehdr.e_type != 2) {
        terminal_writestring("[ELF] Error: File is not an executable\n");
        return -1;
    }

    // Allocate a physical block for the process's page directory
    uint32_t *proc_pd = (uint32_t *)pmm_alloc_block();
    if (!proc_pd) {
        terminal_writestring("[ELF] Error: Failed to allocate Page Directory\n");
        return -1;
    }

    // Copy kernel identity mapping from current page directory
    memcpy(proc_pd, current_page_directory, PMM_BLOCK_SIZE);

    Elf32_Phdr phdr;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        uint32_t offset = ehdr.e_phoff + i * ehdr.e_phentsize;
        if (read_fs(file, offset, sizeof(Elf32_Phdr), (uint8_t *)&phdr) < sizeof(Elf32_Phdr)) {
            terminal_writestring("[ELF] Error: Failed to read program header\n");
            return -1;
        }

        if (phdr.p_type == PT_LOAD) {
            uint32_t start_vaddr = phdr.p_vaddr;
            uint32_t end_vaddr = start_vaddr + phdr.p_memsz;
            uint32_t page_aligned_start = start_vaddr & 0xFFFFF000;
            uint32_t page_aligned_end = (end_vaddr + 4095) & 0xFFFFF000;

            uint32_t *old_pd = current_page_directory;

            // Temporarily switch active page directory to map and load segments
            current_page_directory = proc_pd;
            asm volatile("mov %0, %%cr3" : : "r"(proc_pd) : "memory");

            for (uint32_t vaddr = page_aligned_start; vaddr < page_aligned_end; vaddr += PMM_BLOCK_SIZE) {
                void *phys = pmm_alloc_block();
                if (!phys) {
                    terminal_writestring("[ELF] Error: Failed to allocate physical block for segment mapping\n");
                    return -1;
                }

                // Map virtual page in the active process directory (proc_pd)
                vmm_map_page(phys, (void *)(uintptr_t)vaddr, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
                
                // Zero out the page to clear any previous contents (essential for BSS and alignment)
                memset((void *)(uintptr_t)vaddr, 0, PMM_BLOCK_SIZE);
            }

            // Read the segment content from file directly into the process's mapped virtual address space
            read_fs(file, phdr.p_offset, phdr.p_filesz, (uint8_t *)(uintptr_t)start_vaddr);

            // Restore the old page directory
            current_page_directory = old_pd;
            asm volatile("mov %0, %%cr3" : : "r"(old_pd) : "memory");
        }
    }

    *out_entry = ehdr.e_entry;
    *out_pd = proc_pd;
    return 0;
}
