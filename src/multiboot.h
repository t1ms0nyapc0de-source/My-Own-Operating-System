#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

/* The Multiboot info structure flags. */
#define MULTIBOOT_INFO_MEMORY      0x00000001
#define MULTIBOOT_INFO_MEM_MAP     0x00000040

/*
 * Multiboot Memory Map Entry
 * Describes a region of physical memory.
 */
struct multiboot_mmap_entry {
    uint32_t size;      /* Size of this entry structure (normally 20 or 24 bytes) */
    uint64_t addr;      /* Base physical address of memory region */
    uint64_t len;       /* Size of memory region in bytes */
    uint32_t type;      /* Type of region: 1 = Usable RAM, other = Reserved */
} __attribute__((packed));
typedef struct multiboot_mmap_entry multiboot_memory_map_t;

/*
 * Multiboot Information Structure
 * Passed by the bootloader in EBX.
 */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];       /* Elf section headers or a.out symbol table */
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed));
typedef struct multiboot_info multiboot_info_t;

#endif /* MULTIBOOT_H */
