; isr.s — Interrupt Service Routine stubs
;
; Every IDT entry points to one of these stubs. They all:
;   1. Push a dummy error code (if the CPU does not push one automatically).
;   2. Push the interrupt number.
;   3. Jump to isr_common_stub which saves all registers, calls the C
;      dispatcher isr_dispatch(), then restores everything and returns.

[bits 32]
section .text

extern isr_dispatch     ; C-level dispatcher in idt.c

; -----------------------------------------------------------------------
; isr_common_stub
;
; Entered with the stack looking like:
;   [esp+0]  int_no        (pushed by the individual stub)
;   [esp+4]  err_code      (pushed by CPU or dummy 0 by stub)
;   [esp+8]  eip           (pushed by CPU)
;   [esp+12] cs            (pushed by CPU)
;   [esp+16] eflags        (pushed by CPU)
;   [esp+20] useresp/ss    (pushed by CPU only on Ring 3 -> Ring 0 switch)
; -----------------------------------------------------------------------
isr_common_stub:
    pusha                   ; Push edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov  ax, ds
    push eax                ; Save caller's data segment

    ; Switch to kernel data segment so the handler can access kernel memory
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; Pass pointer to the registers struct as argument
    call isr_dispatch       ; Call C dispatcher
    add  esp, 4             ; Remove the pointer argument

    pop  eax                ; Restore original data segment
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa                    ; Restore edi..eax
    add  esp, 8             ; Discard int_no and err_code
    iret                    ; Return (restores eip, cs, eflags [, esp, ss])

; -----------------------------------------------------------------------
; Macros to generate stub functions
;
; ISR_NOERR: CPU does NOT push an error code for this vector.
;            We push 0 as a placeholder so the stack layout is uniform.
; ISR_ERR:   CPU DOES push an error code automatically.
; -----------------------------------------------------------------------
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0    ; dummy error code
    push dword %1   ; interrupt number
    jmp  isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    ; CPU already pushed the error code; just push the interrupt number
    push dword %1
    jmp  isr_common_stub
%endmacro

; ------- CPU Exceptions 0–31 -------
; Which vectors have a CPU-pushed error code is defined by the x86 ISA.
ISR_NOERR  0    ; #DE  Divide-by-Zero
ISR_NOERR  1    ; #DB  Debug
ISR_NOERR  2    ;      Non-Maskable Interrupt
ISR_NOERR  3    ; #BP  Breakpoint
ISR_NOERR  4    ; #OF  Overflow
ISR_NOERR  5    ; #BR  Bound Range Exceeded
ISR_NOERR  6    ; #UD  Invalid Opcode
ISR_NOERR  7    ; #NM  Device Not Available
ISR_ERR    8    ; #DF  Double Fault          (error code = 0 always)
ISR_NOERR  9    ;      Coprocessor Overrun   (legacy, not used)
ISR_ERR   10    ; #TS  Invalid TSS
ISR_ERR   11    ; #NP  Segment Not Present
ISR_ERR   12    ; #SS  Stack-Segment Fault
ISR_ERR   13    ; #GP  General Protection Fault
ISR_ERR   14    ; #PF  Page Fault
ISR_NOERR 15    ;      Reserved
ISR_NOERR 16    ; #MF  x87 FPU Floating-Point Error
ISR_ERR   17    ; #AC  Alignment Check
ISR_NOERR 18    ; #MC  Machine Check
ISR_NOERR 19    ; #XM  SIMD Floating-Point Exception
ISR_NOERR 20    ; #VE  Virtualization Exception
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30    ; #SX  Security Exception
ISR_NOERR 31

; ------- INT 0x80 — System Call Gate -------
; DPL=3 on the IDT entry lets Ring-3 code trigger this.
ISR_NOERR 128

; ------- Hardware Interrupts (IRQs 0-15) -------
extern irq_dispatch

irq_common_stub:
    pusha                   ; Push edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov  ax, ds
    push eax                ; Save data segment

    ; Switch to kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; Pass registers struct pointer
    call irq_dispatch
    add  esp, 4             ; Clean argument from stack

    pop  eax                ; Restore data segment
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa                    ; Restore general registers
    add  esp, 8             ; Discard dummy error code and interrupt number
    iret                    ; Return and re-enable interrupts

%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0    ; dummy error code
    push dword %2   ; interrupt vector (32 + %1)
    jmp  irq_common_stub
%endmacro

IRQ  0, 32
IRQ  1, 33
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ  10, 42
IRQ  11, 43
IRQ  12, 44
IRQ  13, 45
IRQ  14, 46
IRQ  15, 47
