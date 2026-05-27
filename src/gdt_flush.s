; gdt_flush.s
; Assembly routines to load the GDT and TSS into the CPU.
; Called from C after the GDT tables are built in memory.

[bits 32]
section .text

; -----------------------------------------------------------------------
; gdt_flush(uint32_t gdt_ptr_addr)
;
; Loads the new GDT and reloads all segment registers.
; A far jump (jmp 0x08:.flush) is the only way to reload CS because
; it cannot be written directly with MOV.
; -----------------------------------------------------------------------
global gdt_flush
gdt_flush:
    mov eax, [esp+4]      ; First argument: address of the gdt_ptr struct
    lgdt [eax]            ; Load the GDT

    ; Reload data segment registers with Kernel Data selector (offset 0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with Kernel Code selector (offset 0x08)
    jmp 0x08:.flush
.flush:
    ret

; -----------------------------------------------------------------------
; tss_flush()
;
; Loads the Task Register (TR) with the TSS descriptor selector.
; The TSS is at GDT index 5, so offset = 5 * 8 = 40 = 0x28.
; We OR with 3 (RPL = 3) so the CPU allows Ring-3 tasks to trigger
; the privilege switch through the TSS.
; -----------------------------------------------------------------------
global tss_flush
tss_flush:
    mov ax, 0x2B          ; TSS selector: 0x28 | RPL 3
    ltr ax                ; Load Task Register
    ret
