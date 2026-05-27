; user_switch.s — Ring 0 → Ring 3 privilege switch
;
; enter_user_mode(uint32_t entry, uint32_t user_esp)
;
; The IRET instruction is the only way to drop to a lower privilege ring.
; We fake an interrupt return frame on the stack so the CPU thinks it is
; returning to a Ring-3 program, which causes it to:
;   1. Load CS  from the stack  → switches to the Ring-3 code segment.
;   2. Load SS/ESP from stack   → switches to the user stack.
;   3. Load EFLAGS              → enables interrupts in Ring-3.
;   4. Jump to EIP              → starts executing user_program().
;
; IRET frame layout (pushed in reverse order, top-of-stack last):
;
;   [esp+0]  EIP     — where to jump in Ring 3
;   [esp+4]  CS      — Ring-3 code segment selector (0x1B = 0x18 | RPL 3)
;   [esp+8]  EFLAGS  — CPU flags; bit 9 (IF) must be set to enable IRQs
;   [esp+12] ESP     — Ring-3 stack pointer
;   [esp+16] SS      — Ring-3 stack segment selector (0x23 = 0x20 | RPL 3)

[bits 32]
section .text

global enter_user_mode
enter_user_mode:
    ; Fetch arguments pushed by the C caller
    mov ecx, [esp + 4]   ; ecx = entry (EIP for Ring-3)
    mov edx, [esp + 8]   ; edx = user_esp

    cli                   ; Disable interrupts while we reconfigure segments

    ; Point all data segment registers at the User Data segment (0x23)
    mov ax, 0x23          ; 0x20 (User Data) | RPL 3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; NOTE: SS is set by iret via the frame we push below

    ; Build the iret frame (push in reverse order)
    push 0x23             ; SS  — User Data selector
    push edx              ; ESP — user stack top
    push 0x202            ; EFLAGS: IF=1 (bit 9), reserved bit 1 always set
    push 0x1B             ; CS  — User Code selector (0x18 | RPL 3)
    push ecx              ; EIP — entry point of user_program

    iret                  ; Drop to Ring 3 — does NOT return here
