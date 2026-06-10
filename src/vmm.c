#include "vmm.h"
#include "pmm.h"
#include "string.h"

/* Terminal external helpers for rich diagnostic printing */
extern void terminal_putchar(char c);
extern void terminal_writestring(const char *str);
extern void terminal_writehex(uint32_t n);
extern void terminal_writedec(uint32_t n);

/* 
 * Active Page Directory (must be 4KB aligned)
 */
uint32_t *current_page_directory = NULL;

/* 
 * Assembly control register manipulation and TLB invalidation functions
 */
static inline void write_cr3(uint32_t val) {
    asm volatile("mov %0, %%cr3" : : "r"(val) : "memory");
}

static inline void write_cr0(uint32_t val) {
    asm volatile("mov %0, %%cr0" : : "r"(val) : "memory");
}

static inline uint32_t read_cr0(void) {
    uint32_t val;
    asm volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void invlpg(uint32_t addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

/*
 * vmm_map_page - Maps a virtual address to a physical address.
 */
void vmm_map_page(void *phys_addr, void *virt_addr, uint32_t flags) {
    uint32_t pd_index = ((uint32_t)virt_addr) >> 22;
    uint32_t pt_index = (((uint32_t)virt_addr) >> 12) & 0x3FF;

    uint32_t pde = current_page_directory[pd_index];
    uint32_t *page_table = NULL;

    /* If the corresponding page table is not present, allocate it dynamically */
    if (!(pde & VMM_FLAG_PRESENT)) {
        void *pt_phys = pmm_alloc_block();
        if (!pt_phys) {
            terminal_writestring("[VMM] Critical: Out of physical memory for page table allocation!\n");
            for (;;) { asm volatile("hlt"); }
        }

        /* Zero out the new page table block to clear previous junk entries */
        memset(pt_phys, 0, PMM_BLOCK_SIZE);

        /* 
         * Set the PDE entry:
         * Points to page table physical address, marked as Present, Read/Write, and User-accessible
         * so that the finer PTEs control final privilege levels.
         */
        current_page_directory[pd_index] = ((uint32_t)pt_phys) | VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER;
        page_table = (uint32_t *)pt_phys;
    } else {
        /* Extract page table address (clearing all flags in lower 12 bits) */
        page_table = (uint32_t *)(pde & 0xFFFFF000);
    }

    /* Update the PTE entry with the physical frame base and requested flags */
    page_table[pt_index] = ((uint32_t)phys_addr) | flags;

    /* Invalidate the CPU's Translation Lookaside Buffer (TLB) cache entry for this address */
    invlpg((uint32_t)virt_addr);
}

/*
 * vmm_unmap_page - Disables mapping for a virtual address.
 */
void vmm_unmap_page(void *virt_addr) {
    uint32_t pd_index = ((uint32_t)virt_addr) >> 22;
    uint32_t pt_index = (((uint32_t)virt_addr) >> 12) & 0x3FF;

    uint32_t pde = current_page_directory[pd_index];
    if (pde & VMM_FLAG_PRESENT) {
        uint32_t *page_table = (uint32_t *)(pde & 0xFFFFF000);
        page_table[pt_index] = 0; /* Set entry to non-present */
        
        /* Flush TLB entry */
        invlpg((uint32_t)virt_addr);
    }
}

/*
 * page_fault_handler - IDT 14 Exception handler.
 */
void page_fault_handler(struct registers *regs) {
    /* Read faulting virtual address from CR2 register */
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

    /* Decode error code bits */
    uint32_t err = regs->err_code;
    int present  = err & 0x1;   /* 0: non-present page, 1: protection violation */
    int write    = err & 0x2;   /* 0: read access, 1: write access */
    int user     = err & 0x4;   /* 0: supervisor mode, 1: user mode */
    int rsvd     = err & 0x8;   /* 1: write to reserved bit in PDE/PTE detected */
    int inst     = err & 0x10;  /* 1: fault occurred during instruction fetch */

    /* 
     * Demand Paging Demonstration:
     * Check if the fault is a non-present page within our demand-paging test zone.
     * If so, allocate a physical block, map it, and return to re-execute the instruction!
     */
    if (!present && faulting_address >= 0x1000 && !(user == 0 && faulting_address >= 0xC0000000)) {
        terminal_writestring("[VMM] Caught Page Fault (Demand Paging) at: ");
        terminal_writehex(faulting_address);
        terminal_writestring("\n  -> Status: ");
        if (!present) terminal_writestring("Non-present | ");
        else terminal_writestring("Protection violation | ");
        if (write) terminal_writestring("Write access | ");
        else terminal_writestring("Read access | ");
        if (user) terminal_writestring("User mode\n");
        else terminal_writestring("Supervisor mode\n");

        terminal_writestring("  -> Action: Dynamically allocating physical frame and mapping virtual page...\n");

        void *phys_block = pmm_alloc_block();
        if (!phys_block) {
            terminal_writestring("[VMM] Error: Out of physical memory to satisfy demand paging!\n");
            for (;;) { asm volatile("hlt"); }
        }

        /* Map page in the active directory as Present, Read/Write, User (flags = 0x7) */
        vmm_map_page(phys_block, (void *)(faulting_address & 0xFFFFF000), VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);

        terminal_writestring("  -> Mapped to Physical Frame: ");
        terminal_writehex((uint32_t)phys_block);
        terminal_writestring("\n[VMM] Resuming execution of the interrupted instruction...\n\n");
        return; /* CPU will automatically re-execute the instruction that faulted, which now succeeds! */
    }

    /* Real Memory Access Violation - Trigger Panic */
    terminal_writestring("\n\n*** KERNEL PANIC: Page Fault Memory Access Violation ***\n");
    terminal_writestring("  -> Faulting Address : "); terminal_writehex(faulting_address); terminal_writestring("\n");
    terminal_writestring("  -> Error Code       : "); terminal_writehex(err); terminal_writestring("\n");
    terminal_writestring("  -> Instruction (EIP): "); terminal_writehex(regs->eip); terminal_writestring("\n");
    
    terminal_writestring("  -> Reason           : ");
    if (!present) {
        terminal_writestring("Non-present page (Null/unmapped access)\n");
    } else {
        terminal_writestring("Privilege/Protection violation (Ring 3 writing to Read-Only or accessing Supervisor-only page)\n");
    }

    terminal_writestring("  -> Access Type      : ");
    if (write) {
        terminal_writestring("WRITE\n");
    } else if (inst) {
        terminal_writestring("INSTRUCTION FETCH\n");
    } else {
        terminal_writestring("READ\n");
    }

    terminal_writestring("  -> Privilege Level  : ");
    if (user) {
        terminal_writestring("User Mode (Ring 3)\n");
    } else {
        terminal_writestring("Kernel Mode (Ring 0)\n");
    }

    if (rsvd) {
        terminal_writestring("  -> Error            : Detected write to reserved bits in page table entry!\n");
    }

    terminal_writestring("System Halted.\n");
    for (;;) {
        asm volatile("hlt");
    }
}

/*
 * vmm_init - Setup and enable paging in the CPU.
 */
void vmm_init(void) {
    /* Step 1: Register IDT 14 Exception Gate */
    idt_register_handler(14, page_fault_handler);
    terminal_writestring("[VMM] Registered IDT page fault exception handler (Vector 14).\n");

    /* Step 2: Allocate dynamic Page Directory physical block */
    current_page_directory = (uint32_t *)pmm_alloc_block();
    if (!current_page_directory) {
        terminal_writestring("[VMM] Error: Failed to allocate physical page for Page Directory!\n");
        for (;;) { asm volatile("hlt"); }
    }

    /* Initialize all Page Directory Entries to 0 (non-present) */
    memset(current_page_directory, 0, PMM_BLOCK_SIZE);
    terminal_writestring("[VMM] Allocated and cleared Page Directory at: ");
    terminal_writehex((uint32_t)current_page_directory);
    terminal_writestring("\n");

    /* Step 3: Identity-map all physical RAM detected by PMM */
    uint32_t total_blocks = pmm_get_total_blocks();
    uint32_t total_ram = total_blocks * PMM_BLOCK_SIZE;

    terminal_writestring("[VMM] Flat identity mapping physical memory (");
    terminal_writedec(total_ram / (1024 * 1024));
    terminal_writestring(" MB, ");
    terminal_writedec(total_blocks);
    terminal_writestring(" blocks) as Present, Read/Write, User...\n");

    /* 
     * Map all RAM with Present, Write, and User flags (0x7)
     * This is required since our Ring 3 program and stack are statically loaded inside the kernel.
     */
    for (uint32_t addr = 0; addr < total_ram; addr += PMM_BLOCK_SIZE) {
        vmm_map_page((void *)addr, (void *)addr, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
    }

    /* Step 4: Write directory address to CR3 and enable paging bit in CR0 */
    write_cr3((uint32_t)current_page_directory);

    uint32_t cr0 = read_cr0();
    cr0 |= 0x80000000; /* Set PG (Paging Enable) bit 31 */
    write_cr0(cr0);

    terminal_writestring("[VMM] Paging successfully enabled in CR0 and CR3!\n");
}
