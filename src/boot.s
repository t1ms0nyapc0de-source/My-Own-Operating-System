; Multiboot header constants
MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

; Multiboot header
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Stack setup
section .bss
align 16
global stack_bottom
global stack_top

stack_bottom:
    resb 16384
stack_top:
    
; Entry point
section .text
global _start:function (_start.end - _start)
_start:
    mov esp, stack_top
    
    ; Push Multiboot arguments for kernel_main(magic, mbi_addr)
    push ebx ; second argument: Multiboot info structure pointer
    push eax ; first argument: Multiboot magic number
    
    extern kernel_main
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
.end: